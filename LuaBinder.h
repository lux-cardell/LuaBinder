/*! \file       LuaBinder.h
**  \author     Lux Cardell
**  \date       2/17/2020
**  \brief
**      Tool to automate binding C++ functions to Lua.
**      This file contains the interface
*/
#ifndef LUABINDER_H
#define LUABINDER_H

#include "./lua-5.3.5/src/lua.hpp"
#include <tuple>
#include <string>

/*! Binder
**  \brief
**      Struct wrapping the binding functions
**      Used to allow the binding function to be instantiated off
**      the bound function.
**  \tparam Arg
**      The type the argument should be
*/
template<typename Return, typename...Args>
struct Binder {

    template<Return(Func)(Args...)>
    static int StaticBinding(lua_State * L);

};

#ifndef LUABINDER_INL
#include "LuaBinder.inl"
#endif // !LUABINDER_INL

#endif // !LUABINDER_H

