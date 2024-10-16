## ramrsbd

An example of a Reed-Solomon based error-correcting block device backed
by RAM.

Reed-Solomon codes are sort of the big brother to CRCs. Operating on
bytes instead of bits, they are both flexible and powerful, capable of
detecting and correcting a configurable number of byte errors
efficiently, in $O(ne + e^2)$.

The only tradeoff is they are quite a bit more complex mathematically,
which adds code and RAM cost. TODO measure this.

See also:

- [littlefs][littlefs]
- [ramcrc32bd][ramcrc32bd]

### RAM?

Right now, [littlefs's][littlefs] block device API is limited in terms of
composability. It would be great to fix this on a major API change, but
in the meantime, a RAM-backed block device provides a simple example of
error-correction that users may be able to reimplement in their own
block devices.

### Word of warning

Word of warning! I'm not a mathematician, some of the definitions here
are a bit handwavey, and I'm skipping over the history of BCH and related
codes. I'd encourage you to explore [Wikipedia][wikipedia] and referenced
articles to learn more, though it can get quite dense.

My goal is to explain, to the best of my (simple) knowledge, how to
implement Reed-Solomon codes, and how/why they work.

### How it works

Like CRCs, Reed-Solomon codes are implemented by concatenating the
message with the remainder after division by a predefined "generator
polynomial".

However, two important differences:

1. Instead of using a binary polynomial in [GF(2)][gf2], we use a
   polynomial in a higher-order [finite-field][finite-field], usually
   [GF(256)][gf256] because operating on bytes is convenient.

2. We intentionally construct the polynomial to tell us information about
   any errors that may occur.

   Specifically, we constrain our polynomial (and implicitly our
   codewords) to evaluate to zero at $n$ fixed points. As long as we have
   $e \le \frac{n}{2}$ errors, we can solve for errors using these fixed
   points.

   Note this isn't really possible with CRCs in GF(2), because the only
   non-zero binary number is, well, 1. GF(256) has 255 non-zero elements,
   which we'll see becomes quite important.

#### Constructing a generator polynomial

Ok, first step, constructing a generator polynomial.

If we want to correct $e$ byte-errors, we will need $n = 2e$ fixed
points. We can construct a generator polynomial $P(x)$ with $n$ fixed
points at $g^i$ where $i < n$ like so:

$$
P(x) = \prod_{i=0}^n \left(x - X_i\right)
$$

We could choose any arbitrary set of fixed points, but usually we choose
$g^i$ where $g$ is a [generator][generator] in GF(256), since it provides
a convenient mapping of integers to unique non-zero elements in GF(256).

Note that for any fixed point $g^i$, $x - g^i = g^i - g^i = 0$. And
since multiplying anything by zero is zero, this will make our entire
product zero. So for any fixed point $g^i$, $P(g^i)$ should also evaluate
to zero:

$$
P(g^i) = 0
$$

This gets real nifty when you look at the definition of our Reed-Solomon
code for codeword $C(x)$ given a message $M(x)$:

$$
C(x) = M(x) x^n - (M(x) x^n \bmod P(x))
$$

As is true with normal math, subtracting the remainder after division
gives us a polynomial that is a multiple of $P(x)$. And since multiplying
anything by zero is zero, for any fixed point $g^i$, $C(g^i)$ should also
evaluate to zero:

$$
C(g^i) = 0
$$

#### Modeling errors

Ok, but what if there are errors?

We can think of introducing $e$ errors as adding an error polynomial
$E(x)$ to our original codeword, where $E(x)$ contains $e$ non-zero
terms:

$$
C'(x) = C(x) + E(x)
$$

Check out what happens if we plug in our fixed point, $g^i$:

$$
C'(g^i) = C(g^i) + E(g^i)
        = 0 + E(g^i)
        = E(g^i)
$$

The original codeword drops out! Leaving us with an equation defined only
by the error polynomial.

We call these evaluations our "syndromes" $S_i$, since they tell us
information about the error polynomial:

$$
S_i = C'(g^i) = E(g^i)
$$

We can also give the terms of the error polynomial names. Let's call the
$e$ non-zero terms the "error-magnitudes" $Y_j$:

$$
E(x) = \sum_{j \in e} Y_j x^j
$$

Plugging in our fixed points $g^i$ gives us another definition of our
syndromes $S_i$, which we can rearrange a bit for simplicity. This
results in another set of terms we call the "error-locators" $X_j=g^j$:

$$
S_i = E(g^i) = \sum_{j \in e} Y_j (g^i)^j
             = \sum_{j \in e} Y_j g^(i+j)
             = \sum_{j \in e} Y_j X_j^i
$$

Note that solving for $X_j$ also gives us our "error-locations" $j$,
since $j = \log_g X_j$.

With enough syndromes, and enough math, we can solve for both the error
locations and error magnitudes, which is enough to extract our original
message.

#### Locating the errors

Ok, let's say we received a codeword $C'(x)$ with $e$ errors. Evaluating
at our fixed points $g^i$, where $i < n$ and $n \ge 2e$, gives us our
syndromes $S_i$:

$$
S_i = C'(g^i) = \sum_{j \in e} Y_j X_j^i
$$

The next step is figuring our the locations of our errors $X_j$.

To help with this, we introduce another polynomial, the "error-locator
polynomial" $\Lambda(x)$:

$$
\Lambda(x) = \prod_{j \in e} \left(1 - X_j x\right)
$$

This polynomial has some rather useful properties:

1. For any $X_j$, $\Lambda(X_j^-1) = 0$.

   This is for similar reasons why $P(g^i) = 0$. For any $X_j$,
   $1 - X_j x = 1 - X_j X_j^-1 = 1 - 1 = 0$. And since multiplying
   anything by zero is zero, the product reduces to zero.

2. $\Lambda(0) = 1$.

   This can be seen by plugging in 0:

   $$
   \Lambda(0) = \prod_{j \in e} \left(1 - X_j 0\right)
              = \prod_{j \in e} 1
              = 1
   $$

   And is useful for preventing trivial solutions to $\Lambda(x)$.

But of course we don't know $\Lambda(x)$ yet, we need to find it.

vvvv TODO vvvv

There are several different ways to go about this. In ramrsbd we use
Berlekamp-Massey to iteratively solve for the coefficients of
$\Lambda(x)$ given our syndromes $S_i$.




vvvv scratch vvvv


Let's say we received a codeword $C'(x)$ with $e$ errors. Evaluating at
our fixed points $g^i$, where $i < n$ and $n \ge 2e$ gives us our
syndromes $S_i$:

$$
S_i = C'(g^i) = E(g^i)
$$

$$
E(g^i) = \sum_{j=0}^n Y_j (g^i)^j
       = \sum_{j=0}^n Y_j g^(i+j)
       = \sum_{j=0}^n Y_j X_j^i
$$

$$
S_i = \sum_{j=0}^n Y_j g^(i+j)
$$

The next step if figuring the location of our errors.

To help with this, we introduce another polynomial, the error-locator
polynomial $\Lambda(x)$:

$$
\Lambda(x) = \prod_{j=0}^n \left(1 - X_j x\right)
$$

This polynomial has some rather useful properties:

1. 














We call these evaluations our syndromes, since they 

We call these our syndromes $S_i$, since they tell us information about
the errors. 








This gives us a quick way to check if a codeword is valid. Plug in our
fixed-points $g^i$, and if any result is non-zero, our codeword must
contain an error.

#### 








vvvv scratch vvvv

Ok, first step, constructing a generator polynomial.

If we want to correct $e$ byte-errors, we will need $n = 2e$ fixed
points. We can construct a generator polynomial $P(x)$ with $n$ fixed
points at $g^i$ where $i < n$ like so:

$$
P(x) = \prod_{i=0}^n \left(x - g^i\right)
$$

We could choose any arbitrary set of fixed points, $g^i$ where $g$ is a
[generator][generator] in GF(256) is just a convenient way to map
integers to unique elements in GF(256).

Note that for any fixed point $g^i$, $x - g^i$ evaluates to zero. And
since multiplying anything by zero is zero, this will make our entire
product zero. So for any fixed point $g^i$ where $i < n$:

$$
P(g^i) = 0
$$

The real nifty thing is that concatenation with the remainder after
division, the definition of our Reed-Solomon code:

$$
C(x) = M(x) - (M(x) \bmod P(x))
$$

Gives us a codeword $C(x)$ that is a multiple of $P(x)$. And since
multiplying anything by zero is zero, $C(x)$ should also be zero at our
fixed points:

$$
C(g^i) = 0
$$








, where $g$ is a
[generator][generator] in GF(256)




In Reed-Solomon, we define our generator polynomial $P(x)$ like
so:

$$
P(x) = 
$$



In Reed-Solomon, 



 how do we construct this special generator polynomial?



First note that it's easy to create a polynomial that is zero at a fixed
point $c$:

$$
f(x) = x - c
$$

Multiplying anything by zero is zero, so multiplying anything by $f(x)$
will also be zero at $c$. We can combine a bunch of polynomials this way
to create a polynomial that is zero at any arbitrary points.










defined by a polynomial 

