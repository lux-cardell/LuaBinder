/*! \file       LuaBinder.inl
**  \author     Lux Cardell
**  \date       2/17/2020
**  \brief
**      Implementation file for the Lua binder
*/
#ifndef LUABINDER_INL
#define LUABINDER_INL

#ifndef LUABINDER_H
#include "LuaBinder.h"
#endif // !LUABINDER_H

#include "./lua-5.3.5/src/lua.hpp"

#include <functional>
#include <type_traits>
#include <iostream>
#include <cstring>

#ifndef IS_POINTER
#define IS_POINTER(T, U) \
    typename std::enable_if_t<std::is_pointer<T>::value, U>::type
#endif // !IS_POINTER

/*! LuaValue
**  \brief
**      Generic struct to wrap values from Lua
*/
struct LuaValue {
    //! Enum for the Lua basic types
    enum ValType {
        vtBool,
        vtInt,
        vtNumber,
        vtString,
        vtUserData,
        vtErr
    };

    /*! Constructor
    **  \brief
    **      Constructs a LuaValue by popping a value off the
    **      Lua stack
    */
    LuaValue(lua_State * L, int i);

    /*! GetValue
    **  \brief
    **      Casts the value to the correct type
    */
    template<typename T>
    T GetValue();

    /*! GetValue
    **  \brief
    **      Overload of GetValue for reference types
    */
    template<typename T>
    IS_POINTER(T, T) GetValue();

    //! the type of value being stored
    ValType type;
    //! union to store the data gotten from Lua
    union Value {
        Value() {}
        ~Value() {}
        
        int i;
        float f;
        bool b;
        std::string s;
        void * u;
    } value;
};

// Helper declarations

template<typename Arg>
bool CheckSingleArg(lua_State * L, int i);

template<typename Arg, typename...Rest>
bool CheckArgs(lua_State * L, int i = 1);

template<typename Arg, typename...Rest>
std::tuple<Arg, Rest...> PopArgs(lua_State * L, int i = 1);

template<typename Result>
void PushResult(lua_State * L, Result value);

LuaValue::LuaValue(lua_State * L, int i)
{
    // initialize the union as empty
    std::memset(&value, 0, sizeof(value));
    switch(lua_type(L, i))
    {
        case LUA_TBOOLEAN:
        {
            type = vtBool;
            value.b = lua_toboolean(L, i);
        } break;
        
        case LUA_TNUMBER:
        {
            if (lua_isinteger(L, i)) {
                type = vtInt;
                value.i = lua_tointeger(L, i);
            }
            else {
                type = vtNumber;
                value.f = lua_tonumber(L, i);
            }
        } break;
        
        case LUA_TSTRING:
        {
            type = vtString;
            value.s = std::string(lua_tostring(L, i));
        } break;
        
        case LUA_TLIGHTUSERDATA:
        case LUA_TUSERDATA:
        {
            type = vtUserData;
            value.u = lua_touserdata(L, i);
        } break;
        
        default: {
            std::cout << "Type not supported by Lua Binder\n";
            type = ValType::vtErr;
        } break;
    }
}

template<>
int LuaValue::GetValue()
{
    if (type == vtNumber)
        return static_cast<int>(value.f);
    return value.i;
}

template<>
float LuaValue::GetValue()
{
    if (type == vtInt)
        return static_cast<float>(value.i);
    return value.f;
}

template<>
bool LuaValue::GetValue()
{
    return value.b;
}

template<>
std::string LuaValue::GetValue()
{
    return value.s;
}

template<>
void * LuaValue::GetValue()
{
    return value.u;
}

template<typename T>
bool CheckSingleArg(lua_State * L, int i)
{
    return lua_isuserdata(L, i) || lua_islightuserdata(L, i);
}

template<>
bool CheckSingleArg<int>(lua_State * L, int i)
{
    return lua_isinteger(L, i);
}

template<>
bool CheckSingleArg<float>(lua_State * L, int i)
{
    return lua_isnumber(L, i);
}

template<>
bool CheckSingleArg<std::string>(lua_State * L, int i)
{
    return lua_isstring(L, i);
}

template<>
bool CheckSingleArg<bool>(lua_State * L, int i)
{
    return lua_isboolean(L, i);
}

template<typename Arg, typename...Rest>
bool CheckArgs(lua_State * L, int i)
{
    if (!CheckSingleArg<Arg>(L, i))
        return false;
    if constexpr (sizeof...(Rest) == 0)
        return true;
    else
        return CheckArgs<Rest...>(L, i+1);
}

template<typename Arg, typename...Rest>
std::tuple<Arg, Rest...> PopArgs(lua_State * L, int i)
{
    LuaValue val(L, i);
    std::tuple<Arg> a;
    if constexpr (std::is_pointer<Arg>::value == true)
    {
        std::get<0>(a) = reinterpret_cast<Arg>(val.GetValue<void*>());
    }
    else
    {
        std::get<0>(a) = val.GetValue<Arg>();
    }

    if constexpr (sizeof...(Rest) == 0)
        return a;
    else
        return std::tuple_cat(a, PopArgs<Rest...>(L, i+1));
}

template<>
void PushResult(lua_State * L, void * value)
{
    lua_pushlightuserdata(L, value);
}

template<>
void PushResult(lua_State * L, int value)
{
    lua_pushinteger(L, value);
}

template<>
void PushResult(lua_State * L, float value)
{
    lua_pushnumber(L, value);
}

template<>
void PushResult(lua_State * L, bool value)
{
    lua_pushboolean(L, value);
}

template<>
void PushResult(lua_State * L, std::string value)
{
    lua_pushstring(L, value.c_str());
}

template<typename Return, typename...Args>
template<Return(Func)(Args...)>
int Binder<Return, Args...>::StaticBinding(lua_State * L)
{
    const int stack = lua_gettop(L);
    if (stack != sizeof...(Args)) {
        std::cout << "Incorrect argument count\n";
        return 0;
    }
    if constexpr (sizeof...(Args) != 0)
    {
        if (CheckArgs<Args...>(L) == false) {
            std::cout << "Incorrect argument type\n";
            return 0;
        }
    }
    if constexpr (sizeof...(Args) == 0) {
        if constexpr (std::is_void<Return>::value == true) {
            Func();
            return 0;
        }
        else {
            Return result = Func();
            PushResult(L, result);
            return 1;
        }
    }
    else {
        std::tuple<Args...> args = PopArgs<Args...>(L);
        if constexpr (std::is_void<Return>::value == true) {
            std::apply(Func, args);
            return 0;
        }
        else {
            Return result = std::apply(Func, args);
            PushResult(L, result);
            return 1;
        }
    }
}

#endif // !LUABINDER_INL

