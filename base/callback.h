#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#include <functional>
#include <memory>
#include <tuple>

#include "base/check.h"

// The code in this file is inspired from Chromium's //base implementation [1],
// countless Stack Overflow articles that helped me (Dom Farolino) understand
// some pretty complex language features, and lots of messages with Daniel Cheng
// (dcheng@chromium.org) that I'm very grateful for.
//
// [1]: https://source.chromium.org/chromium/chromium/src/+/main:base/bind.h

namespace base {

// `BindStateBase` only exists for type erasure -- that is, to allow `OnceClosure`
// to hold onto a `BindState` while being blind to its bound return type and
// argument types.
class BindStateBase {
 public:
  BindStateBase() = default;
  virtual ~BindStateBase() = default;
  virtual void Invoke() = 0;
};

template <typename Functor, typename... Args>
class BindState final : public BindStateBase {
  // Note that we always decay the types that go into `BindState` so that
  // `BindState` retains value semantics (i.e., doesn't hold onto implicit
  // references of any kind). See documentation in `BindOnce()` for more details
  // on what this entails. This means that we can assert that the given functor
  // type and every argument type is not a reference of any kind (i.e., is
  // properly decayed by `BindOnce()`).
  static_assert(std::is_same<std::decay_t<Functor>, Functor>::value);
  static_assert((std::is_same_v<std::decay_t<Args>, Args> && ...));

 public:
  BindState() = delete;
  ~BindState() override = default;
  template <typename FwdFunctor,
            typename... FwdArgs,
            typename =
                std::enable_if_t<(std::is_convertible_v<FwdArgs&&, Args> && ...)>>
  BindState(FwdFunctor&& f, FwdArgs&&... args)
      : f_(std::forward<FwdFunctor>(f)), args_(std::forward<FwdArgs>(args)...) {}

  BindState(const BindState& other) = delete;
  BindState& operator=(const BindState& other) = delete;
  BindState(BindState&& other) = default;
  BindState& operator=(BindState&& other) = default;

  void Invoke() override {
    auto seq =
      std::make_index_sequence<std::tuple_size<decltype(args_)>::value>{};
    InvokeImpl(std::move(f_), std::move(args_), seq);
  }

  template <typename BoundFunctor, typename BoundArgs, size_t... Is>
  void InvokeImpl(BoundFunctor&& f, BoundArgs&& args,
                  std::index_sequence<Is...>) {
    // Note that `Invoke` always *moves* the functor and argument tuple into
    // this method; this means that both `BoundFunctor` and `BoundArgs` are
    // always non-reference types due to the universal reference template type
    // deduction rules.
    static_assert(std::is_same_v<std::decay_t<BoundFunctor>, Functor>);
    static_assert(std::is_same_v<std::decay_t<BoundArgs>, BoundArgs>);

    // Note that below we do a lot of perfect forwarding on `BoundFunctor` and
    // `BoundArgs` as it is best practice. However, since in practice both types
    // are non-reference types, `std::forward` is equivalent to `std::move`
    // here. This means that we're always passing the argument tuple to
    // `std::get<Is>()` by rvalue, which means it will return an rvalue to each
    // argument accordingly. We'll invoke the functor with each argument passed
    // by rvalue, which makes `InvokeImpl` "prefer" moving arguments into
    // functor parameters when possible. That is, if a given functor param type
    // is movable (i.e., implements a move constructor) the argument is moved
    // from the tuple into the functor parameter. Otherwise, the parameter's
    // copy constructor will be called with the rvalue (which is allowed, since
    // the copy constructor's parameter should be a const reference), and the
    // argument will by copied from the argument tuple to the functor parameter.
    if constexpr (std::is_member_function_pointer<BoundFunctor>::value) {
      std::mem_fn(std::forward<BoundFunctor>(f))
          (std::get<Is>(std::forward<BoundArgs>(args)) ...);

    } else {
      std::forward<BoundFunctor>(f)
          (std::get<Is>(std::forward<BoundArgs>(args)) ...);
    }
  }

 private:
  Functor f_;
  std::tuple<Args...> args_;
};

class OnceClosure;

template <typename Functor, typename... Args>
OnceClosure BindOnce(Functor&& f, Args&&... args);

// The `OnceClosure` class is just a light wrapper around `BindStateBase`. Once
// we introduce a generic `Callback` class that supports partially-bound
// functors and consumers supplying unbound arguments at invocation time,
// `OnceClosure` can be a specialization of that class.
class OnceClosure {
 public:
  OnceClosure() = default;
  explicit OnceClosure(std::unique_ptr<BindStateBase> bind_state) :
      bind_state_(std::move(bind_state)) {}
  OnceClosure(const OnceClosure& other) = delete;
  OnceClosure& operator=(const OnceClosure& other) = delete;
  OnceClosure(OnceClosure&& other) = default;
  OnceClosure& operator=(OnceClosure&& other) = default;

  // Converting constructor that delegates to the move constructor above with
  // `OnceClosure` object constructed from `BindOnce`. This is just so we can
  // support assign `OnceClosure = [](){};` directly to a lambda function. It is
  // intentional that this constructor is not marked as `explicit` as it is a
  // converting constructor.
  // We have to conditionally enable this constructor so that it won't
  // accidentally be triggered when we try and construct-assign a `OnceClosure`
  // to another `OnceClosure`. Otherwise that operation would trigger this
  // constructor over the deleted copy constructor, and crash.
  template <typename Lambda,
          typename = typename std::enable_if_t<
              !std::is_same_v<std::decay_t<Lambda>, OnceClosure>>>
  OnceClosure(Lambda&& lambda) :
      OnceClosure(BindOnce(std::forward<Lambda>(lambda))) {}

  void operator()() {
    CHECK(bind_state_);
    bind_state_->Invoke();
    bind_state_.reset();
  }

  operator bool() {
    return !!bind_state_;
  }

 private:
  std::unique_ptr<BindStateBase> bind_state_;
};

template <typename Functor, typename... Args>
OnceClosure BindOnce(Functor&& f, Args&&... args) {
  // Always declare decayed types when passing functors and arguments into
  // `BindState`, so that it retains value semantics. This means that for
  // example, functor arguments passed in as:
  //   - Lvalues --> just get copied normally into `BindState::args_`, as if
  //     there was no type decaying at all.
  //   - Lvalue references --> get demoted to normal values, and copied into
  //     `BindState::args_`
  //   - Rvalue references --> get demoted to normal values, and the
  //     originally-passed-in object is *moved* into the instance of the demoted
  //     type that is stored in, you guessed it, `BindState::args_`. That is if
  //     you pass in std::move(foo) as an argument to `BindOnce()`, the type
  //     of the argument in `BindState::args_` tuple will be decayed from
  //     `Foo&&` to `Foo`, and `BindState::args_` will look like
  //     `std::tuple<T1, ..., Foo, ...>`. But we perfectly forward all of the
  //     arguments from this method into `BindState::ctor()`. That way when we
  //     construct the tuple over there, we construct an object of type `Foo`
  //     from an object of type `Foo&&`, which *moves* the originally-passed-in
  //     object into an instance stored in the tuple.
  //
  // In order to pass in an lvalue reference and have it be treated as an actual
  // reference at invocation time (thus modifying the value that was originally
  // passed in), you have to declare that you're explicitly passing in an owned
  // reference by using `std::ref()`. This is the same way that `std::bind()`,
  // `std::thread()`, and other APIs work.
  using BindState = BindState<std::decay_t<Functor>, std::decay_t<Args>...>;
  std::unique_ptr<BindStateBase> bind_state =
      std::make_unique<BindState>(
          std::forward<Functor>(f), std::forward<Args>(args)...);
  return OnceClosure(std::move(bind_state));
}

using Predicate = std::function<bool()>;

} // namespace base

#endif // BASE_CALLBACK_H_
