// Fixture for synthaxes-decimal-in-s. Self-contained; S is the wrap macro (its double form).
#define S(x) x
#define NEAR(a, b, t) (((a) - (b)) < (t)) // stands in for EXPECT_NEAR & friends
#define HALF 0.5                          // a constant macro whose body has a bare literal

using sample = double;

sample wrapped()
{
    return S(0.5);
} // OK  — wrapped in S

sample wrappedExpr()
{
    return S(2.0 * 4.0);
} // OK  — both wrapped by the one S

int integer()
{
    return 5;
} // OK  — integer literal, not floating

sample fromBody()
{
    return HALF;
} // OK here — the literal lives in HALF's body

sample bare()
{
    return 0.5;
} // BAD — bare -> S(0.5)

sample bareExpr()
{
    return 1.0 + 2.0;
} // BAD x2

sample bareExp()
{
    return 1e-5;
} // BAD — exponent form is still floating

bool inOtherMacro(sample a, sample b)
{
    return NEAR(a, b, 3e-4);
} // BAD — arg of another macro
