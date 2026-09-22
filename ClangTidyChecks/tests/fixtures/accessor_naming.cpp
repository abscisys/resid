// Fixture for synthaxes-accessor-naming. Self-contained. A member function named like a
// private/protected m_ field is flagged only when its signature matches the field's type: a getter
// takes nothing and returns that type (by value, reference or pointer); a setter returns void and
// takes one parameter of that type. A bool field's getter is is<Field>(), not get<Field>(). Names
// already in accessor form, and idiomatic container names (size, count, ...), are left alone.

struct Blob
{
    int v;
};

struct Base
{
    virtual ~Base() = default;
    virtual int frequency() const = 0; // the name is dictated here, at the root of the contract
};

class Widget : public Base
{
public:
    // BAD -- value getter returns the field's type -> getResistance().
    double resistance() const
    {
        return this->m_resistance;
    }

    // BAD -- setter returns void and takes one parameter of the field's type -> setResistance().
    void resistance(double value)
    {
        this->m_resistance = value;
    }

    // BAD -- reference getter still corresponds to the field -> getMeta().
    const Blob& meta() const
    {
        return this->m_meta;
    }

    // BAD -- pointer getter corresponds to the field -> getHandle().
    Blob* handle()
    {
        return &this->m_handle;
    }

    // BAD -- a bool field's getter reads is<Field>() -> isEnabled().
    bool enabled() const
    {
        return this->m_enabled;
    }

    // OK -- get/set prefix already present (name differs from the field).
    double getWidth() const
    {
        return this->m_width;
    }

    void setWidth(double value)
    {
        this->m_width = value;
    }

    // OK -- the field is m_isReady, so isReady() is already the correct is<Field>() form.
    bool isReady() const
    {
        return this->m_isReady;
    }

    // OK -- an idiomatic container name in the ignore list stays bare, not getSize().
    int size() const
    {
        return this->m_size;
    }

    // OK -- name matches m_tag but the return type (long) is not the field's type (int).
    long tag() const
    {
        return 0;
    }

    // OK -- shares m_undo's name but is a command: wrong signature (two params, returns bool).
    bool undo(int& id, int amount)
    {
        return this->m_undo > id + amount;
    }

    // OK -- no data member named m_process exists.
    void process() {}

    // OK -- override; the name comes from Base, flag it there, not on every implementation.
    int frequency() const override
    {
        return this->m_frequency;
    }

private:
    double m_resistance = 0.0;
    Blob m_meta{};
    Blob m_handle{};
    bool m_enabled = false;
    double m_width = 0.0;
    bool m_isReady = false;
    int m_size = 0;
    int m_tag = 0;
    int m_undo = 0;
    int m_frequency = 0;
};

// OK -- a public data member is not the private/protected accessor pattern the check targets.
struct Point
{
    int x() const
    {
        return m_x;
    }

public:
    int m_x = 0;
};
