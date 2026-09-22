#pragma once

// Our objective is to construct a smooth interpolating single-valued function
// y = f(x).
//
// Catmull-Rom splines are widely used for interpolation, however these are
// parametric curves [x(t) y(t) ...] and can not be used to directly calculate
// y = f(x).
// For a discussion of Catmull-Rom splines see Catmull, E., and R. Rom,
// "A Class of Local Interpolating Splines", Computer Aided Geometric Design.
//
// Natural cubic splines are single-valued functions, and have been used in
// several applications e.g. to specify gamma curves for image display.
// These splines do not afford local control, and a set of linear equations
// including all interpolation points must be solved before any point on the
// curve can be calculated. The lack of local control makes the splines
// more difficult to handle than e.g. Catmull-Rom splines, and real-time
// interpolation of a stream of data points is not possible.
// For a discussion of natural cubic splines, see e.g. Kreyszig, E., "Advanced
// Engineering Mathematics".
//
// Our approach is to approximate the properties of Catmull-Rom splines for
// piecewice cubic polynomials f(x) = ax^3 + bx^2 + cx + d as follows:
// Each curve segment is specified by four interpolation points,
// p0, p1, p2, p3.
// The curve between p1 and p2 must interpolate both p1 and p2, and in addition
//   f'(p1.x) = k1 = (p2.y - p0.y)/(p2.x - p0.x) and
//   f'(p2.x) = k2 = (p3.y - p1.y)/(p3.x - p1.x).
//
// The constraints are expressed by the following system of linear equations
//
//   [ 1  xi    xi^2    xi^3 ]   [ d ]    [ yi ]
//   [     1  2*xi    3*xi^2 ] * [ c ] =  [ ki ]
//   [ 1  xj    xj^2    xj^3 ]   [ b ]    [ yj ]
//   [     1  2*xj    3*xj^2 ]   [ a ]    [ kj ]
//
// Solving using Gaussian elimination and back substitution, setting
// dy = yj - yi, dx = xj - xi, we get
//
//   a = ((ki + kj) - 2*dy/dx)/(dx*dx);
//   b = ((kj - ki)/dx - 3*(xi + xj)*a)/2;
//   c = ki - (3*xi*a + 2*b)*xi;
//   d = yi - ((xi*a + b)*xi + c)*xi;
//
// Having calculated the coefficients of the cubic polynomial we have the
// choice of evaluation by brute force
//
//   for (x = x1; x <= x2; x += res) {
//     y = ((a*x + b)*x + c)*x + d;
//     plot(x, y);
//   }
//
// or by forward differencing
//
//   y = ((a*x1 + b)*x1 + c)*x1 + d;
//   dy = (3*a*(x1 + res) + 2*b)*x1*res + ((a*res + b)*res + c)*res;
//   d2y = (6*a*(x1 + res) + 2*b)*res*res;
//   d3y = 6*a*res*res*res;
//
//   for (x = x1; x <= x2; x += res) {
//     plot(x, y);
//     y += dy; dy += d2y; d2y += d3y;
//   }
//
// See Foley, Van Dam, Feiner, Hughes, "Computer Graphics, Principles and
// Practice" for a discussion of forward differencing.
//
// If we have a set of interpolation points p0, ..., pn, we may specify
// curve segments between p0 and p1, and between pn-1 and pn by using the
// following constraints:
//   f''(p0.x) = 0 and
//   f''(pn.x) = 0.
//
// Substituting the results for a and b in
//
//   2*b + 6*a*xi = 0
//
// we get
//
//   ki = (3*dy/dx - kj)/2;
//
// or by substituting the results for a and b in
//
//   2*b + 6*a*xj = 0
//
// we get
//
//   kj = (3*dy/dx - ki)/2;
//
// Finally, if we have only two interpolation points, the cubic polynomial
// will degenerate to a straight line if we set
//
//   ki = kj = dy/dx;
//


/// Selects the segment interpolator used by interpolate(): true evaluates the cubic at every
/// point (interpolateBruteForce), false uses forward differencing (interpolateForwardDifference).
inline constexpr bool kSplineBruteForce = false;

/// Computes the cubic f(x) = ax^3 + bx^2 + cx + d through (x1, y1) and (x2, y2) with slopes
/// k1 and k2 at those points.
/// @param x1 X of the first point.
/// @param y1 Y of the first point.
/// @param x2 X of the second point; must differ from @p x1.
/// @param y2 Y of the second point.
/// @param k1 Slope f'(x1).
/// @param k2 Slope f'(x2).
/// @param[out] a Cubic coefficient.
/// @param[out] b Quadratic coefficient.
/// @param[out] c Linear coefficient.
/// @param[out] d Constant term.
inline void cubicCoefficients(double x1, double y1, double x2, double y2, double k1, double k2, double& a, double& b, double& c, double& d)
{
    double dx = x2 - x1, dy = y2 - y1;

    a = ((k1 + k2) - 2 * dy / dx) / (dx * dx);
    b = ((k2 - k1) / dx - 3 * (x1 + x2) * a) / 2;
    c = k1 - (3 * x1 * a + 2 * b) * x1;
    d = y1 - ((x1 * a + b) * x1 + c) * x1;
}

/// Plots one cubic curve segment between x1 and x2, evaluating the polynomial at each step.
/// @tparam PointPlotter Callable taking (double x, double y).
/// @param x1 X of the segment start.
/// @param y1 Y of the segment start.
/// @param x2 X of the segment end.
/// @param y2 Y of the segment end.
/// @param k1 Slope at the segment start.
/// @param k2 Slope at the segment end.
/// @param plot Receives each computed point.
/// @param res Step between plotted x values.
template <class PointPlotter> inline void interpolateBruteForce(double x1, double y1, double x2, double y2, double k1, double k2, PointPlotter plot, double res)
{
    double a, b, c, d;
    cubicCoefficients(x1, y1, x2, y2, k1, k2, a, b, c, d);

    // Calculate each point.
    for(int i = 0;; ++i)
    {
        const double x = x1 + i * res;
        if(x > x2)
        {
            break;
        }
        double y = ((a * x + b) * x + c) * x + d;
        plot(x, y);
    }
}

/// Plots one cubic curve segment between x1 and x2 using forward differencing: three additions
/// per point instead of a polynomial evaluation. This is the default segment interpolator.
/// @tparam PointPlotter Callable taking (double x, double y).
/// @param x1 X of the segment start.
/// @param y1 Y of the segment start.
/// @param x2 X of the segment end.
/// @param y2 Y of the segment end.
/// @param k1 Slope at the segment start.
/// @param k2 Slope at the segment end.
/// @param plot Receives each computed point.
/// @param res Step between plotted x values.
template <class PointPlotter> inline void interpolateForwardDifference(double x1, double y1, double x2, double y2, double k1, double k2, PointPlotter plot, double res)
{
    double a, b, c, d;
    cubicCoefficients(x1, y1, x2, y2, k1, k2, a, b, c, d);

    double y = ((a * x1 + b) * x1 + c) * x1 + d;
    double dy = (3 * a * (x1 + res) + 2 * b) * x1 * res + ((a * res + b) * res + c) * res;
    double d2y = (6 * a * (x1 + res) + 2 * b) * res * res;
    double d3y = 6 * a * res * res * res;

    // Calculate each point.
    for(int i = 0;; ++i)
    {
        const double x = x1 + i * res;
        if(x > x2)
        {
            break;
        }
        plot(x, y);
        y += dy;
        dy += d2y;
        d2y += d3y;
    }
}

/// X coordinate of an interpolation point.
/// @param p Iterator to a two-element point.
/// @return (*p)[0].
template <class PointIter> inline double x(PointIter p)
{
    return (*p)[0];
}

/// Y coordinate of an interpolation point.
/// @param p Iterator to a two-element point.
/// @return (*p)[1].
template <class PointIter> inline double y(PointIter p)
{
    return (*p)[1];
}

/// Plots a smooth single-valued curve through a sequence of points (Catmull-Rom-like cubic
/// segments; see the file comment).
///
/// Since each curve segment is controlled by four points, the end points will not be
/// interpolated. If extra control points are not desirable, the end points can simply be
/// repeated to ensure interpolation. Points of non-differentiability and discontinuity can be
/// introduced by repeating points.
/// @tparam PointIter Forward iterator to two-element (x, y) points, sorted by x.
/// @tparam PointPlotter Callable taking (double x, double y).
/// @param p0 First point.
/// @param pn Last point (inclusive).
/// @param plot Receives each computed point.
/// @param res Step between plotted x values.
template <class PointIter, class PointPlotter> inline void interpolate(PointIter p0, PointIter pn, PointPlotter plot, double res)
{
    double k1, k2;

    // Set up points for first curve segment.
    PointIter p1 = p0;
    ++p1;
    PointIter p2 = p1;
    ++p2;
    PointIter p3 = p2;
    ++p3;

    // Draw each curve segment.
    for(; p2 != pn; ++p0, ++p1, ++p2, ++p3)
    {
        // p1 and p2 equal; single point.
        if(x(p1) == x(p2))
        {
            continue;
        }
        // Both end points repeated; straight line.
        if(x(p0) == x(p1) && x(p2) == x(p3))
        {
            k1 = k2 = (y(p2) - y(p1)) / (x(p2) - x(p1));
        }
        // p0 and p1 equal; use f''(x1) = 0.
        else if(x(p0) == x(p1))
        {
            k2 = (y(p3) - y(p1)) / (x(p3) - x(p1));
            k1 = (3 * (y(p2) - y(p1)) / (x(p2) - x(p1)) - k2) / 2;
        }
        // p2 and p3 equal; use f''(x2) = 0.
        else if(x(p2) == x(p3))
        {
            k1 = (y(p2) - y(p0)) / (x(p2) - x(p0));
            k2 = (3 * (y(p2) - y(p1)) / (x(p2) - x(p1)) - k1) / 2;
        }
        // Normal curve.
        else
        {
            k1 = (y(p2) - y(p0)) / (x(p2) - x(p0));
            k2 = (y(p3) - y(p1)) / (x(p3) - x(p1));
        }

        if constexpr(kSplineBruteForce)
        {
            interpolateBruteForce(x(p1), y(p1), x(p2), y(p2), k1, k2, plot, res);
        }
        else
        {
            interpolateForwardDifference(x(p1), y(p1), x(p2), y(p2), k1, k2, plot, res);
        }
    }
}

namespace synthaxes::hw::engine::sid
{

    /// Plotter for interpolate() that stores each point into an array: arr[F(x)] = F(y).
    /// @tparam F Element type of the target array.
    template <class F> class PointPlotter
    {
    protected:
        F* m_f;

    public:
        /// Constructs a plotter writing into @p arr.
        /// @param arr Target array, indexed by x; must cover every plotted x.
        PointPlotter(F* arr): m_f(arr) {}

        /// Stores one point, clamping negative y values to zero.
        /// @param x Array index.
        /// @param y Value to store.
        void operator()(double x, double y)
        {
            // Clamp negative values to zero.
            if(y < 0)
            {
                y = 0;
            }

            this->m_f[F(x)] = F(y);
        }
    };

} // namespace synthaxes::hw::engine::sid
