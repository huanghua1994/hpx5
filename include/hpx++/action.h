// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef ACTION_H
#define ACTION_H

extern "C" {
  #include "hpx/addr.h"
  #include "hpx/types.h"
  #include "hpx/action.h"
  #include "hpx/rpc.h"
}

#include <hpx++/lco.h>
#include <hpx++/runtime.h>

#include <cstdlib>
#include <tuple>
#include <type_traits>

namespace hpx {
  
  namespace detail {
    // reference: https://functionalcpp.wordpress.com/2013/08/05/function-traits/
    template<class F>
    struct function_traits;
    
    // function pointer
    template<class R, class... Args>
    struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
    {};

    template<class R, class... Args>
    struct function_traits<R(&)(Args...)> : public function_traits<R(Args...)>
    {};

    template<class R, class... Args>
    struct function_traits<R(Args...)> {
	using return_type = R;
	static constexpr std::size_t arity = sizeof...(Args);
	using arg_types = std::tuple<Args...>;
	static constexpr auto arg_types_tpl = std::tuple<Args...>();
    };
    
    // reference: http://stackoverflow.com/questions/17424477/implementation-c14-make-integer-sequence
    // using aliases for cleaner syntax
    template<class T> using Invoke = typename T::type;

    template<unsigned...> struct seq{ using type = seq; };

    template<class S1, class S2> struct concat;

    template<unsigned... I1, unsigned... I2>
    struct concat<seq<I1...>, seq<I2...>>
      : seq<I1..., (sizeof...(I1)+I2)...>{};

    template<class S1, class S2>
    using Concat = Invoke<concat<S1, S2>>;

    template<unsigned N> struct gen_seq;
    template<unsigned N> using GenSeq = Invoke<gen_seq<N>>;

    template<unsigned N>
    struct gen_seq : Concat< GenSeq<N/2>, GenSeq<N - N/2> >{};

    template<> struct gen_seq<0> : seq<>{};
    template<> struct gen_seq<1> : seq<0>{};
    
    template <typename T>
    inline
    ::std::tuple<decltype(HPX_POINTER), decltype(HPX_SIZE_T)>
    xform(T&& t) {
      return ::std::make_tuple(HPX_POINTER, HPX_SIZE_T);
    }
    template <typename T>
    inline
    ::std::tuple<T*, ::std::size_t>
    convert_arg(T& arg) {
      return ::std::make_tuple(&arg, sizeof(T));
    }
    
    template <typename A>
    struct action_struct {
      /*
      * 
      /// @param        type The type of the action (THREAD, TASK, INTERRUPT, ...).
      /// @param        attr The attribute of the action (PINNED, PACKED, ...).
      /// @param     handler The action handler (the function).
      /// @param          id The action id (the hpx_action_t address).
      /// @param __VA_ARGS__ The parameter types (HPX_INT, ...).
      #define HPX_REGISTER_ACTION(type, attr, id, handler, ...)
      */
      // for 0 args, HPX_REGISTER_ACTION is not HPX_MARSHALLED
      template <typename F, typename Tpl>
      static int _register_helper(F f, Tpl&& t, seq<>&& s) {
	return hpx_register_action(HPX_DEFAULT, HPX_ATTR_NONE, __FILE__ ":" _HPX_XSTR(A::id), 
				   &(A::id), (hpx_action_handler_t) f);
      }
      template <typename F, typename Tpl, unsigned... Is>
      static int _register_helper(F f, Tpl&& t, seq<Is...>&& s) {
	return hpx_register_action(HPX_DEFAULT, HPX_MARSHALLED, __FILE__ ":" _HPX_XSTR(A::id), 
				   &(A::id), (hpx_action_handler_t) f, 
				   sizeof...(Is) , ::std::get<Is>(t)...);
      }
      
      template <typename F, unsigned... Is>
      static int _register(F f, seq<Is...>&& s) {
	auto tpl = ::std::tuple_cat(xform(::std::get<Is>(A::traits::arg_types_tpl))...);
	return _register_helper(f, tpl, gen_seq<::std::tuple_size<decltype(tpl)>::value>());
      }
      
      template <typename R, typename Tpl, unsigned... Is>
      int _call_sync_helper(hpx_addr_t& addr, R& result, Tpl&& tpl, hpx::detail::seq<Is...>&& s) {
	return _hpx_call_sync(addr, A::id, &result, sizeof(R), sizeof...(Is), ::std::get<Is>(tpl)...);
      }
      
      template <typename R, typename... Args>
      int call_sync(hpx_addr_t& addr, R& result, Args... args) {
	auto tpl = ::std::tuple_cat(hpx::detail::convert_arg(args)...);
	return _call_sync_helper(addr, result, tpl, hpx::detail::gen_seq<::std::tuple_size<decltype(tpl)>::value>());
      }
      
      template <typename Tpl, unsigned... Is>
      int _run_helper(Tpl&& tpl, hpx::detail::seq<Is...>&& s) {
	return hpx::run(&(A::id), ::std::get<Is>(tpl)...);
      }
      
      template <typename... Args>
      int operator()(Args... args) {
	static_assert(::std::is_same< typename A::traits::arg_types, ::std::tuple<Args...> >::value,
		      "action and argument types do not match");
	auto tpl = ::std::tuple_cat(hpx::detail::convert_arg(args)...);
	return _run_helper(tpl, hpx::detail::gen_seq<::std::tuple_size<decltype(tpl)>::value>());
      }

    };
    
    template <typename T, typename F>
    inline
    int _register_action(F f) {
      return T::_register(f, hpx::detail::gen_seq<::std::tuple_size<decltype(T::traits::arg_types_tpl)>::value>());
    }
    
  }
  
}

#define HPXPP_REGISTER_ACTION(f)						\
struct f##_action_struct : public hpx::detail::action_struct<f##_action_struct> {\
  static hpx_action_t id;							\
  using traits = hpx::detail::function_traits<decltype(f)>;			\
};										\
hpx_action_t f##_action_struct::id = 0;						\
int f##_action_struct_dummy = hpx::detail::_register_action<f##_action_struct>(f);

#endif