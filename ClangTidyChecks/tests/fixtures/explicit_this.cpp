// Fixture for synthaxes-explicit-this. Self-contained (no #includes) so it parses with just
// -std=c++17, no system-header search path. The comments mark what the check must and must not do.

struct Widget
{
    int m_value;
    static int s_count;

    int compliantMember()
    {
        return this->m_value;
    } // OK  — explicit this->

    int violationMember()
    {
        return m_value;
    } // BAD — bare member    -> this->

    int violationCall()
    {
        return helper();
    } // BAD — bare method call -> this->

    int helper()
    {
        return 0;
    }

    int compliantStatic()
    {
        return Widget::s_count;
    } // OK  — explicit Widget::

    int violationStatic()
    {
        return s_count;
    } // BAD — bare static     -> Widget::
};

int Widget::s_count = 0;
