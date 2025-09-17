// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Ast.h"
#include "Luau/Variant.h"

namespace Luau
{

/// Pretty much an IR instruction node, but for simplicity we just hijack the
/// AST and allow synthetic opcodes. It can be really tempting to forego the AST
/// and go all-in on the IR approach, but that's for another day. You could
/// envision the simplicity in effect tracking analysis when you see following
/// programs all normalize to the same IR.
/// ```
/// local t0 = { p = 5, q = 7 }
///
/// local t1 = {}
/// t1.p = 5
/// t1.q = 7
///
/// -- if we let f : ({ write p: number, write q: number }) -> ()
/// local t2 = {}
/// f(t2)
/// ```
/// These three subprograms map to the same essence that the IR captures,
/// albeit with some minor differences, e.g. `__newindex` for the second,
/// `__call` for the third.
/// ```
/// newtable
/// push "p"
/// push 5
/// settablek t
/// push "q"
/// push 7
/// settablek t
/// ```
/// The type inference engine already does this anyhow, just implicitly and
/// manually hand-craft the equivalence via C++ code.
///
/// Either way, we will try to keep it simple for now, but we do _need_ some
/// notion of adding instructions that isn't directly expressible in the AST nor
/// is it syntactically obvious via pattern matching. I have a strong distaste
/// for syntactic pattern matching when I want the semantics to justify and
/// prove the case. (cough: that's how you get C++. Sigh, `T&&` is either lvalue
/// reference or universal reference, depending on the de Bruijn index or whether
/// `T` is even a template type parameter. Sigh.). That said, pragmatically even
/// I cheat out of necessity, either by deadlines or by Rice's theorem. That's
/// why it's tempting to forego the AST and construct a whole universe of IR
/// nodes...
struct Instruction
{
    struct Stat
    {
        explicit Stat(AstStat* stat)
            : stat(stat)
        {
        }

        AstStat* stat;
    };

    struct Expr
    {
        explicit Expr(AstExpr* expr)
            : expr(expr)
        {
        }

        AstExpr* expr;
    };

    /// This acts as a "poison" instruction that, despite the syntax, proves the
    /// rest of the instructions to be inconsequential to the program state.
    struct Unreachable
    {
        explicit Unreachable() = default;
    };

    using Op = Variant<Stat, Expr, Unreachable>;

    template<typename T, typename = std::enable_if_t<Op::is_part_of_v<std::decay_t<T>>>>
    Instruction(T&& opcode)
        : opcode(std::forward<T>(opcode))
    {
    }

    template<typename T>
    const T* get() const
    {
        return opcode.get_if<T>();
    }

private:
    Op opcode;
};

} // namespace Luau
