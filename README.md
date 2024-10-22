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

### Testing

Testing is a bit jank right now, relying on littlefs's test runner:

``` bash
$ git clone https://github.com/littlefs-project/littlefs -b v2.9.3 --depth 1
$ make test -j
```

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
             = \sum_{j \in e} Y_j g^{ij}
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

   This prevents trivial solutions and is what makes $\Lambda(x)$ useful.

But what's _really_ interesting, is that these two properties allow us to
solve for $\Lambda(x)$ with only our syndromes $S_i$.

First note that since $\Lambda(x)$ has $e$ roots, we can expand it into
an $e$ degree polynomial. We also know that $\Lambda(0) = 1$, so the
constant term must be 1. If we name the coefficients of this polynomial
$\Lambda_k$, this gives us another definition of $\Lambda(x)$:

$$
\Lambda(x) = 1 + \sum_{k=1}^e \Lambda_k x^k
$$

Plugging in $X_j^{-1}$ should still evaluate to zero:

$$
\Lambda(X_j^{-1}) = 1 + \sum_{k=1}^e \Lambda_k X_j^{-k} = 0
$$

And since multiplying anything by zero is zero, we can multiply this by,
say, $Y_j X_j^i$, and the result should still be zero:

$$
Y_j X_j^i \Lambda(X_j^{-1}) = Y_j X_j^i + \sum_{k=1}^e Y_j X_j^{i-k} \Lambda_k = 0
$$

We can even add a bunch of these together and the result should still be
zero:

$$
\sum_{j \in e} Y_j X_j^i \Lambda(X_j^{-1}) = \sum_{j \in e} \left(Y_j X_j^i + \sum_{k=1}^e Y_j X_j^{i-k} \Lambda_k\right) = 0
$$

Wait a second...

$$
\sum_{j \in e} Y_j X_j^i \Lambda(X_j^{-1}) = \left(\sum_{j \in e} Y_j X_j^i\right) + \sum_{k=1}^e \left(\sum_{j \in e} Y_j X_j^{i-k}\right) \Lambda_k = 0
$$

Aren't these our syndromes? $S_i$?

$$
\sum_{j \in e} Y_j X_j^i \Lambda(X_j^{-1}) = S_i + \sum_{k=1}^e S_{i-k} \Lambda_k = 0
$$

We can rearrange this into an equation for $S_i$ using only our
coefficients and $e$ previously seen syndromes $S_{i-k}$:

$$
S_i = \sum_{k=1}^e S_{i-k} \Lambda_k
$$

The only problem is this is one equation with $e$ unknowns, our
coefficients $\Lambda_k$.

But if we repeat this for $e$ syndromes, $S_{e}$ to $S_{n-1}$, we can
build $e$ equations for $e$ unknowns, and create a system of equations
that is solvable. This is why we need $n=2e$ syndromes/fixed-points to
solve for $e$ errors:

$$
\begin{bmatrix}
S_{e} \\
S_{e+1} \\
\vdots \\
S_{n-1} \\
\end{bmatrix} =
\begin{bmatrix}
S_{e-1} & S_{e-2} & \dots & S_0\\
S_{e} & S_{e-1} & \dots & S_1\\
\vdots & \vdots & \ddots & \vdots \\
S_{n-2} & S_{n-3} & \dots & S_{e-1}\\
\end{bmatrix}
\begin{bmatrix}
\Lambda_1 \\
\Lambda_2 \\
\vdots \\
\Lambda_e \\
\end{bmatrix}
$$

#### Berlekamp-Massey

Ok that's the theory, but solving this system of equations efficiently is
still quite difficult.

Enter the Berlekamp-Massey algorithm.

The key observation here by Massey, is that solving for $\Lambda(x)$ is
equivalent to constructing an LFSR that when given the initial sequence
$S_0, S_1, \dots, S_{e-1}$, generates the sequence
$S_e, S_{e+1}, \dots, S_{n-1}$:

```
.---- + <- + <- + <- + <--- ... --- + <--.
|     ^    ^    ^    ^              ^    |
|     |    |    |    |              |    |
|    *Λ1  *Λ2  *Λ3  *Λ4     ...   *Λe-1 *Λe
|     ^    ^    ^    ^              ^    ^
|   .-|--.-|--.-|--.-|--.--     --.-|--.-|--.
'-> |Se-1|Se-2|Se-3|Se-4|   ...   | S1 | S0 | ->
    '----'----'----'----'--     --'----'----'

                     |
                     v

.---- + <- + <- + <- + <--- ... --- + <--.
|     ^    ^    ^    ^              ^    |
|     |    |    |    |              |    |
|    *Λ1  *Λ2  *Λ3  *Λ4     ...   *Λe-1 *Λe
|     ^    ^    ^    ^              ^    ^
|   .-|--.-|--.-|--.-|--.--     --.-|--.-|--.
'-> |Sn-1|Sn-2|Sn-3|Sn-4|   ...   |Se+1| Se | ->
    '----'----'----'----'--     --'----'----'
```

```
    .----.----.----.----.--     --.----.----.
.-> |Se-1|Se-2|Se-3|Se-4|   ...   | S1 | S0 |
|   '-|--'-|--'-|--'-|--'--     --'-|--'-|--'
|     v    v    v    v              v    v
|    *Λ1  *Λ2  *Λ3  *Λ4     ...   *Λe-1 *Λe
|     |    |    |    |              |    |
|     v    v    v    v              v    |
'---- + <- + <- + <- + <--- ... --- + <--'

                     |
                     v

    .----.----.----.----.--     --.----.----.
.-> |Sn-1|Sn-2|Sn-3|Sn-4|   ...   |Se+1| Se |
|   '-|--'-|--'-|--'-|--'--     --'-|--'-|--'
|     v    v    v    v              v    v
|    *Λ1  *Λ2  *Λ3  *Λ4     ...   *Λe-1 *Λe
|     |    |    |    |              |    |
|     v    v    v    v              v    |
'---- + <- + <- + <- + <--- ... --- + <--'
```

Berlekamp-Massey relies on two key observations:

1. If an LFSR $L$ of size $|L|$ generates the sequence
   $s_0, s_1, \dots, s_{n-1}$, but failed to generate the sequence
   $s_0, s_1, \dots, s_{n-1}, s_n$, than an LFSR $L'$ that generates the
   sequence must have a size of at least:

   $$
   |L'| \ge n+1-|L|
   $$

   Massey's proof of this gets a bit wild.

   Consider the equation for our LFSR $L$:
   
   $$
   s_n = \sum_{k=1}^{|L|} L_k s_{n-k}
   $$

   If we have another LFSR $L'$ that generates
   $s_{n-|L|}, s_{n-|L|+1}, \cdots, s_{n-1}$, we can substitute it in for
   $s_{n-k}$:

   $$
   s_n = \sum_{k=1}^{|L|} L_k \sum_{k'=1}^{|L'|} L'_{k'} s_{n-k-k'}
   $$

   Multiplication in is distributive, so we can move our summations
   around:
   
   $$
   s_n = \sum_{k'=1}^{|L'|} L'_{k'} \sum_{k=1}^{|L|} L_k s_{n-k-k'}
   $$

   Note the right summation looks like $L$. If $L$ generates
   $s_{n-|L'|}, s_{n-|L'|+1}, \cdots, s_{n-1}$, we can replace it with
   $s_{n-k'}$:
   
   $$
   s_n = \sum_{k'=1}^{|L'|} L'_{k'} s_{n-k'}
   $$
   
   Oh hey! That's the definition of $L'$. So if $L'$ generates $s_n$,
   $L$ also generates $s_n$.

   The only way to make $L'$ generate a different $s_n$ would be to make
   $|L'| \ge n+1-|L|$ so that $L$ can no longer generate
   $s_{n-|L'|}, s_{n-|L'|+1}, \cdots, s_{n-1}$.

2. Once we've found the best LFSR $L$ for a given size $|L|$, its
   definition provides an optimal strategy for changing only the last
   element of the generated sequence.

   This is assuming $L$ failed of course. If $L$ generated the whole
   sequence our algorithm is done!

   If $L$ failed, we assume it correctly generated
   $s_0, s_1, \cdots, s_{n-1}$, but failed at $s_n$. We call the
   difference from the expected symbol the discrepancy $d$:

   $$  
   L(i) = \sum_{k=1}^{|L|} L_k s_{i-k} =  
   \begin{cases}  
   s_i & i = |L|, |L|+1, \cdots, n-1 \\  
   s_i+d & i = n  
   \end{cases}  
   $$  

   If we know $s_i$ (which requires a larger LFSR), we can rearrange this
   to be a bit more useful. We call this our connection polynomial $C$:

   $$  
   C(i) = d^{-1}\left(s_i - \sum_{k=1}^{|L|} L_k s_{i-k}\right) =  
   \begin{cases}  
   0 & i = |L|, |L|+1,\cdots,n-1 \\  
   1 & i = n  
   \end{cases}  
   $$

   Now, if we have a larger LFSR $L'$ with size $|L'| \gt |L|$, and we
   want to change only the symbol $s'_n$ by $d'$, we can just add
   $d' C(i)$ to it:

   $$
   L'(i) + d' C(i) =
   \begin{cases}
   s'_i & i = |L'|,|L'|+1,\cdots,n-1 \\
   s'_i + d' & i = n
   \end{cases}
   $$

If you can wrap your head around those two observations, you'll have
understood most of Berlekamp-Massey.

The actual algorithm itself is relatively simple:  
  
1. Using the current LFSR $L$, generate the next symbol $s'_n$, and
   calculate the discrepancy $d$ between $s'_n$ and the expected symbol
   $s_n$:

   $$
   d = s'_n - s_n
   $$  
  
2. If $d=0$, great! Move on to the next symbol.  
  
3. If $d \ne 0$, we need to tweak our LFSR.

   1. First check if our LFSR is big enough. If $n \ge 2|L|$, we need a
      bigger LFSR:

      $$
      |L'| = n+1-|L|
      $$

      If we're changing the size, save the current LFSR for future
      tweaks:

      $$
      C'(i) = d^{-1} L(i)
      $$

      $$
      m = n
      $$

   2. Now we can fix the LFSR by adding our last $C$ (not $C'$!),
      shifting and scaling so only $s_n$ is affected:

      $$
      L'(i) = L(i) + d C(i-(n-m))
      $$

      Though usually we don't bother to track $m$ explicitly, we
      can instead shift $C$ by 1 every step so it ends up in the right
      location.

This is all implemented in [ramrsbd_find_l][ramrsbd_find_l].


### Solving binary LFSRs for fun and profit

Taking a step away from GF(256) for a moment, let's look at a simpler
LFSR in GF(2), aka binary.

Consider this binary sequence generated by a minimal LFSR that I know and
you don't :)

```
1 1 0 0 1 1 1 1
```

Can you figure out the original LFSR?

---

To start with Berlekamp-Massey, let's assume our LFSR is an empty LFSR
that just spits out a stream of zeros. Not the most creative LFSR, but we
have to start somewhere!

```
|L0| = 0
L0(i) = 0
C0(i) = s_i

L0 = 0 -> Output:   0
          Expected: 1
                d = 1
```

Ok, so immediately we see a discrepancy. Clearly our output is not a
string of all zeros, and we need _some_ LFSR:

```
|L1| = 0+1-|L0| = 1
L1(i) = L0(i) + C0(i-1) = s_i-1
C1(i) = s_i + L0(i) = s_i

     .-----.
     |     |
     |   .-|--.
L1 = '-> | 1  |-> Output:   1 1
         '----'   Expected: 1 1
                        d = 0
```

That's looking much better. This little LFSR will actually get us
decently far into the sequence:

```
|L2| = |L1| = 1
L2(i) = L1(i) = s_i-1
C2(i) = C1(i-1) = s_i-1

     .-----.
     |     |
     |   .-|--.
L2 = '-> | 1  |-> Output:   1 1 1
         '----'   Expected: 1 1 1
                        d = 0
```

```
|L3| = |L2| = 1
L3(i) = L2(i) = s_i-1
C3(i) = C2(i-1) = s_i-2

     .-----.
     |     |
     |   .-|--.
L3 = '-> | 1  |-> Output:   1 1 1 1
         '----'   Expected: 1 1 1 1
                        d = 0
```

```
|L4| = |L3| = 1
L4(i) = L3(i) = s_i-1
C4(i) = C3(i-1) = s_i-3

     .-----.
     |     |
     |   .-|--.
L4 = '-> | 1  |-> Output:   1 1 1 1 1
         '----'   Expected: 0 1 1 1 1
                        d = 1
```

Ah! A discrepancy!

We're now at step 4 with only a 1-bit LFSR. $4 \ge 2\cdot1$, so a bigger
LFSR is needed.

Resizing our LFSR to $4+1-1 = 4$, we can then add $C(i-1)$ to fix the
discrepancy, save the previous LFSR as the new $C(i)$, and continue:

```
|L5| = 4+1-|L4| = 4
L5(i) = L4(i) + C4(i-1) = s_i-1 + s_i-4
C5(i) = s_i + L4(i) = s_i + s_i-1

     .---- + <------------.
     |     ^              |
     |   .-|--.----.----.-|--.   Expected: 0 0 1 1 1 1
L5 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 0 1 1 1 1
         '----'----'----'----'         d = 1
```

Another discrepancy! This time we don't need to resize the LFSR, just add
the shifted $C(i-1)$.

Thanks to math, we know this should have no affect on any of the
previously generated symbols, but feel free to regenerate the sequence to
prove this to yourself. This property is pretty unintuitive!

```
|L6| = |L5| = 4
L6(i) = L5(i) + C5(i-1) = s_i-2 + s_i-4
C6(i) = C5(i-1) = s_i-1 + s_i-2

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.
L6 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 0 0 1 1 1 1
         '----'----'----'----'   Expected: 1 0 0 1 1 1 1
                                       d = 0
```

No discrepancy this time, let's keep going:

```
|L7| = |L6| = 4
L7(i) = L6(i) = s_i-2 + s_i-4
C7(i) = C6(i-1) = s_i-2 + s_i-3

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.
L7 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 1 0 0 1 1 1 1
         '----'----'----'----'   Expected: 1 1 0 0 1 1 1 1
                                       d = 0
```

And now that we've generated the whole sequence, we have our LFSR:

```
|L8| = |L7| = 4
L8(i) = L7(i) = s_i-2 + s_i-4

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.
L8 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 1 0 0 1 1 1 1
         '----'----'----'----'   Expected: 1 1 0 0 1 1 1 1
```

In case you want to play around with it, I've ported this algorithm to
python in [bm-lfsr-solver.py][bm-lfsr-solver.py]. Feel free to try your
own binary sequences to get a feel for the algorithm:

``` bash
$ ./bm-lfsr-solver.py 1 1 0 0 1 1 1 1

... snip ...

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.
L8 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 1 0 0 1 1 1 1
         '----'----'----'----'   Expected: 1 1 0 0 1 1 1 1
```

```
$ ./bm-lfsr-solver.py 01101000 01101001 00100001

... snip ...

      .---- + <---------------- + <- + <- + <------ + <-------.
      |     ^                   ^    ^    ^         ^         |
      |   .-|--.----.----.----.-|--.-|--.-|--.----.-|--.----.-|--.----.
L24 = '-> | 1  | 0  | 0  | 1  | 0  | 0  | 1  | 0  | 0  | 0  | 0  | 1  |-> Output:   0 1 1 0 1 0 0 0 0 1 1 0 1 0 0 1 0 0 1 0 0 0 0 1
          '----'----'----'----'----'----'----'----'----'----'----'----'   Expected: 0 1 1 0 1 0 0 0 0 1 1 0 1 0 0 1 0 0 1 0 0 0 0 1
```


I've also implemented a similar script for full GF(256) LFSRs, though
it's a bit harder for follow unless you can do full GF(256)
multiplications in your head!

```
$ ./bm-lfsr256-solver.py 30 80 86 cb a3 78 8e 00

... snip ...

     .---- + <- + <- + <--.
     |     ^    ^    ^    |
     |    *f0  *04  *df  *ea
     |     ^    ^    ^    ^
     |   .-|--.-|--.-|--.-|--.
L8 = '-> | a3 | 78 | 8e | 00 |-> Output:   30 80 86 cb a3 78 8e 00
         '----'----'----'----'   Expected: 30 80 86 cb a3 78 8e 00
```

Is this a good compression algorithm? Probably not.






vvvv TODO vvvv


Thanks to math, we know this should have no affect on any of the
previously generated symbols. Feel free to re


The first question is what size LFSR do we need now, clearly a 1-bit LFSR
is too small.

The first interesting observation in Berlekamp-Massey is that if an LFSR
of size $n$ generated the sequence $s_0, s_1, \dots, s_{i-1}$, but failed
to generate the sequence $s_0, s_1, \dots, s_{i-1}, s_i$, than any LFSR
that does generate the sequence must have a size of at least
$n' \ge i+1 - n$.

Recall the mathematical definition of our LFSR:





```
n = 4
L5(x) = L4(x) + T4(x) x = 1 + x + x^4
T5(x) = L4(x) = 1 + x

     .---- + <------------.
     |     ^              |
     |   .-|--.----.----.-|--.   Expected: 0 0 1 1 1 1
L5 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 0 1 1 1 1
         '----'----'----'----'         d = 1

L6(x) = L5(x) + T5(x) x = 1 + x^2 + x^4
T6(x) = T5(x) x = x + x^2

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.   Expected: 1 0 0 1 1 1 1
L6 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 0 0 1 1 1 1
         '----'----'----'----'         d = 0

L7(x) = L6(x) = 1 + x^2 + x^4
T7(x) = T6(x) x = x^2 + x^3

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.   Expected: 1 1 0 0 1 1 1 1
L7 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 1 0 0 1 1 1 1
         '----'----'----'----'         d = 0

L8(x) = L7(x) = 1 + x^2 + x^4
T8(x) = T7(x) x = x^3 + x^4
```
















vvvv TODO vvvv

To start, Berlekamp-Massey assumes the simplest LFSR, a constant stream
of zeros. Yes, this is a bit silly, but we need to start somewhere:

```
L = 0 ->
```

Evaluating the first output, we immediately see a problem, $1 \ne 0$. We
call this the discrepancy:


```
|L0| = 0
L0(x) = 1
T0(x) = 1

          Expected: 1
L0 = 0 -> Output:   0
                d = 0
```

Ok, so clearly we need some LFSR, since our output is not a string of all
zeros. The next simplest LFSR to try is a 1-bit LFSR:

```
n = 1
L1(x) = L0(x) + T0(x) x = 1 + x
T1(x) = L0(x) = 1

     .-----.
     |     |
     |   .-|--.   Expected: 1 1
L1 = '-> | 1  |-> Output:   1 1
         '----'         d = 0
```

So far so good, this little LFSR will actually get us decently far into
the sequence:

```
n = 1
L2(x) = L1(x) = 1 + x
T2(x) = T1(x) x = x

     .-----.
     |     |
     |   .-|--.   Expected: 1 1 1
L2 = '-> | 1  |-> Output:   1 1 1
         '----'         d = 0

n = 1
L3(x) = L2(x) = 1 + x
T3(x) = T2(x) x = x^2

     .-----.
     |     |
     |   .-|--.   Expected: 1 1 1 1
L3 = '-> | 1  |-> Output:   1 1 1 1
         '----'         d = 0

n = 1
L4(x) = L3(x) = 1 + x
T4(x) = T3(x) x = x^3

     .-----.
     |     |
     |   .-|--.   Expected: 0 1 1 1 1
L4 = '-> | 1  |-> Output:   1 1 1 1 1
         '----'         d = 1
```

A discrepancy!

The first question is what size LFSR do we need now, clearly a 1-bit LFSR
is too small.

The first interesting observation in Berlekamp-Massey is that if an LFSR
of size $n$ generated the sequence $s_0, s_1, \dots, s_{i-1}$, but failed
to generate the sequence $s_0, s_1, \dots, s_{i-1}, s_i$, than any LFSR
that does generate the sequence must have a size of at least
$n' \ge i+1 - n$.

Recall the mathematical definition of our LFSR:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} \ne s_i
$$

If we assume there does exist another LFSR $L'_k$ of size
$n' \lt i+1 - n$ that generates the sequence
$s_0, s_1, \dots, s_{i-1}, s_i$, we can substitute it in for each
$s_{i-(k+1)}$:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = \sum_{k=0}^{n-1} L_k \sum_{k'=0}^{n'-1} L'_k s_{i-(k+1)-(k'+1)}
$$

And since these are summations, we can rearrange things:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = \sum_{k'=0}^{n'-1} L'_k \sum_{k=0}^{n-1} L_k s_{i-(k'+1)-(k+1)}
$$

And use the definition of our original LFSR to replace the last
summation, since all of these $s_{i-(k'+1)}$ are before $s_i$:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = \sum_{k'=0}^{n'-1} L'_k s_{i-(k'+1)}
$$

Which then reduces to $s_i$ thanks to our second LFSR:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = s_i
$$

So if there exists an LFSR of size $n' \lt i+1 - n$ that generates $s_i$,
our original LFSR must have also been able to generate $s_i$.

---

So since we're on step $i=4$ with an LFSR of size $n=1$, the new LFSR
must be at least $n=4$.







vvvv TODO vvvv

   We can use previously found, smaller LFSRs, to to build a longer LFSR
   without affecting existing results.

   First note that we know the expected symbols $s_i$, so we can rewrite our LFSR equation:

   $$
   s_i = \sum_{k=1}^{|L|}L_ks_{i-k}
   $$

   Like so:

   $$
   0 = s_i - \sum_{k=1}^{|L|}L_ks_{i-k}
   $$

   If an LFSR $L$ failed at $s_n$, then $L$ generates $0$ for all $i \lt n$, and generates $d$ for $i = n$.

   $$
   d = s_n - \sum_{k=1}^{|L|}L_ks_{n-k}
   $$

   We can manipulate this to any value $v$ by dividing by $d$ and multiplying by $v$:

   $$
   v = v d^{-1} \left(s_n - \sum_{k=1}^{|L|}L_ks_{n-k}\right)
   $$

TODO




#### Solving an LFSR

Taking a step away from GF(256), let's look at a simpler example in
GF(2), aka binary.

Consider this binary LFSR:

```
.--------- + <-------.
|          ^         |
|   .----.-|--.----.-|--.
'-> | 1  | 1  | 1  | 1  |-> 
    '----'----'----'----'
      0    1    1    1      1
      0    0    1    1      1    1
      1    0    0    1      1    1    1
      1    1    0    0      1    1    1    1

L(x) = 1 + x^2 + x^4
```

Consider this binary sequence, generated by an unknown LFSR:

```
1 1 0 0 1 1 1 1
```

To start, Berlekamp-Massey assumes the simplest LFSR, a constant stream
of zeros. Yes, this is a bit silly, but we need to start somewhere:

```
L = 0 ->
```

Evaluating the first output, we immediately see a problem, $1 \ne 0$. We
call this the discrepancy:


```
n = 0
L0(x) = 1
T0(x) = 1

          Expected: 1
L0 = 0 -> Output:   0
                d = 0
```

Ok, so clearly we need some LFSR, since our output is not a string of all
zeros. The next simplest LFSR to try is a 1-bit LFSR:

```
n = 1
L1(x) = L0(x) + T0(x) x = 1 + x
T1(x) = L0(x) = 1

     .-----.
     |     |
     |   .-|--.   Expected: 1 1
L1 = '-> | 1  |-> Output:   1 1
         '----'         d = 0
```

So far so good, this little LFSR will actually get us decently far into
the sequence:

```
n = 1
L2(x) = L1(x) = 1 + x
T2(x) = T1(x) x = x

     .-----.
     |     |
     |   .-|--.   Expected: 1 1 1
L2 = '-> | 1  |-> Output:   1 1 1
         '----'         d = 0

n = 1
L3(x) = L2(x) = 1 + x
T3(x) = T2(x) x = x^2

     .-----.
     |     |
     |   .-|--.   Expected: 1 1 1 1
L3 = '-> | 1  |-> Output:   1 1 1 1
         '----'         d = 0

n = 1
L4(x) = L3(x) = 1 + x
T4(x) = T3(x) x = x^3

     .-----.
     |     |
     |   .-|--.   Expected: 0 1 1 1 1
L4 = '-> | 1  |-> Output:   1 1 1 1 1
         '----'         d = 1
```

A discrepancy!

The first question is what size LFSR do we need now, clearly a 1-bit LFSR
is too small.

The first interesting observation in Berlekamp-Massey is that if an LFSR
of size $n$ generated the sequence $s_0, s_1, \dots, s_{i-1}$, but failed
to generate the sequence $s_0, s_1, \dots, s_{i-1}, s_i$, than any LFSR
that does generate the sequence must have a size of at least
$n' \ge i+1 - n$.

Recall the mathematical definition of our LFSR:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} \ne s_i
$$

If we assume there does exist another LFSR $L'_k$ of size
$n' \lt i+1 - n$ that generates the sequence
$s_0, s_1, \dots, s_{i-1}, s_i$, we can substitute it in for each
$s_{i-(k+1)}$:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = \sum_{k=0}^{n-1} L_k \sum_{k'=0}^{n'-1} L'_k s_{i-(k+1)-(k'+1)}
$$

And since these are summations, we can rearrange things:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = \sum_{k'=0}^{n'-1} L'_k \sum_{k=0}^{n-1} L_k s_{i-(k'+1)-(k+1)}
$$

And use the definition of our original LFSR to replace the last
summation, since all of these $s_{i-(k'+1)}$ are before $s_i$:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = \sum_{k'=0}^{n'-1} L'_k s_{i-(k'+1)}
$$

Which then reduces to $s_i$ thanks to our second LFSR:

$$
\sum_{k=0}^{n-1} L_k s_{i-(k+1)} = s_i
$$

So if there exists an LFSR of size $n' \lt i+1 - n$ that generates $s_i$,
our original LFSR must have also been able to generate $s_i$.

---

So since we're on step $i=4$ with an LFSR of size $n=1$, the new LFSR
must be at least $n=4$.











The observation by Massey is that if an LFSR failed at step $i$, the size
of the correct LFSR must be at least $n'$ where:

$$
n' = i+1 - n
$$

He proves this by contradiction.

First note we have a failing LFSR of size $n$ with coefficients $L_k$.
Failing implies it returns $S_j$ up until the current step $j = i$:

$$  
\sum_{k=0}^{n-1} L_k S_{j-k}
\begin{cases}
= S_{j+1} & n \le j \lt i \\
\ne S_{j+1} & j = i
\end{cases}
$$

Let's assume there exists a non-failing LFSR of size $n' \lt i+1 -n$:

$$  
\sum_{k'=0}^{n'-1} L'_{k'} S_{j-k'}
\begin{cases}
= S_{j+1} & n' \le j \le i
\end{cases}
$$

Looking at specifically $j=i$:

$$
\sum_{k=0}^{n-1} L_k S_{i-k}
$$

Since $i-k \le i$, we can replace $S_{i-k}$ with our second LFSR:

$$
\sum_{k=0}^{n-1} L_k S_{i-k} = \sum_{k=0}^{n-1} L_k \sum_{k'=0}^{n'-1} L'_{k'} S_{i-k-1-k'}
$$

Moving things around:

$$
\begin{aligned}
\sum_{k=0}^{n-1} L_k S_{i-k} &= \sum_{k=0}^{n-1} L_k \sum_{k'=0}^{n'-1} L'_{k'} S_{i-k-1-k'}
\\ &= \sum_{k'=0}^{n'-1} L'_{k'} \sum_{k=0}^{n-1} L_k S_{i-k'-1-k}
\\ &= \sum_{k'=0}^{n'-1} L'_{k'} S_{i-k'}
\\ &= S_{i+1}
\end{aligned}
$$

Which contradicts our original assumption that the original LFSR $\ne S_{i+1}$









$$
S_{j+1} = \sum_{i=0}^n L_i S_{j-i}
$$



```
n = 4
L5(x) = L4(x) + T4(x) x = 1 + x + x^4
T5(x) = L4(x) = 1 + x

     .---- + <------------.
     |     ^              |
     |   .-|--.----.----.-|--.   Expected: 0 0 1 1 1 1
L5 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 0 1 1 1 1
         '----'----'----'----'         d = 1

L6(x) = L5(x) + T5(x) x = 1 + x^2 + x^4
T6(x) = T5(x) x = x + x^2

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.   Expected: 1 0 0 1 1 1 1
L6 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 0 0 1 1 1 1
         '----'----'----'----'         d = 0

L7(x) = L6(x) = 1 + x^2 + x^4
T7(x) = T6(x) x = x^2 + x^3

     .--------- + <-------.
     |          ^         |
     |   .----.-|--.----.-|--.   Expected: 1 1 0 0 1 1 1 1
L7 = '-> | 1  | 1  | 1  | 1  |-> Output:   1 1 0 0 1 1 1 1
         '----'----'----'----'         d = 0

L8(x) = L7(x) = 1 + x^2 + x^4
T8(x) = T7(x) x = x^3 + x^4
```

```
     .-----.
     |     |
     |   .-|--.   Expected: 1 1
L1 = '-> | 1  |-> Output:   1 1
         '----'         d = 0
L1(x) = 1x
T1(x) = 1x
```

```
     .-----.
     |     |
     |   .-|--.   Expected: 1 1 1 1
L1 = '-> | 1  |-> Output:   1 1 1 1
         '----'
L1(x) = 1x
```


```
.---- + <------ + <--.
|     ^         ^    |
|   .-|--.----.-|--.-|--.
'-> | 0  | 0  | 0  | 1  |-> 
    '----'----'----'----'
      1    0    0    0      1
      1    1    0    0      0    1
      1    1    1    0      0    0    1
      0    1    1    1      0    0    0    1

L(x) = 1 + x + x^3 + x^4
```

Consider this binary sequence, generated by an unknown LFSR:

```
0 1 1 1 0 0 0 1
```


```
L0(x) = 1
T0(x) = 1

          Expected: 1
L0 = 0 -> Output:   0
                d = 0

L1(x) = L0(x) + T0(x) x = 1 + x
T1(x) = L0(x) = 1

     .-----.
     |     |
     |   .-|--.   Expected: 0 1
L1 = '-> | 1  |-> Output:   1 1
         '----'         d = 1

L2(x) = L1(x) + T1(x) x = 1
T2(x) = L1(x) = 1 + x

          .----.----.   Expected: 0 0 1
L2 = 0 -> | 0  | 1  |-> Output:   0 0 1
          '----'----'         d = 0

L3(x) = L2(x) = 1
T3(x) = T2(x) x = x + x^2

          .----.----.   Expected: 0 0 0 1
L3 = 0 -> | 0  | 1  |-> Output:   0 0 0 1
          '----'----'         d = 0

L4(x) = L3(x) = 1
T4(x) = T3(x) x = x^2 + x^3

          .----.----.   Expected: 1 0 0 0 1
L4 = 0 -> | 0  | 1  |-> Output:   0 0 0 0 1
          '----'----'         d = 1

L5(x) = L4(x) + T4(x) x = 1 + x^3 + x^4
T5(x) = L4(x) = 1

     .-------------- + <--.
     |               ^    |
     |   .----.----.-|--.-|--.   Expected: 1 1 0 0 0 1
L5 = '-> | 0  | 0  | 0  | 1  |-> Output:   0 1 0 0 0 1
         '----'----'----'----'         d = 1

L6(x) = L5(x) + T5(x) x = 1 + x + x^3 + x^4
T6(x) = L5(x) = 1 + x^3 + x^4

     .---- + <------ + <--.
     |     ^         ^    |
     |   .-|--.----.-|--.-|--.   Expected: 1 1 1 0 0 0 1
L6 = '-> | 0  | 0  | 0  | 1  |-> Output:   1 1 1 0 0 0 1
         '----'----'----'----'         d = 0

L7(x) = L6(x) = 1 + x + x^3 + x^4
T7(x) = T6(x) x = x + x^4 + x^5

     .---- + <------ + <--.
     |     ^         ^    |
     |   .-|--.----.-|--.-|--.   Expected: 0 1 1 1 0 0 0 1
L7 = '-> | 0  | 0  | 0  | 1  |-> Output:   0 1 1 1 0 0 0 1
         '----'----'----'----'         d = 0

L8(x) = L7(x) = 1 + x + x^3 + x^4
T8(x) = T7(x) x = x^2 + x^3 + x^6
```



To understand the algorithm, let's use an example.

I have the following syndromes, $S_0, S_1, \dots, S_7$:

```
S[0:8] = 00 8e 78 a3 cb 86 80 30
```

Let's see if we can figure out the LFSR that matches the syndromes.

step 0:

```
S = 00
E = 00
d = 00
e = 0

                                            .----.
                                            |    |
                                            '-|--'
                                              v   
L = 00                                  T =  x1
                                              |   
                                              |   
                                            <-'   
```

step 1:

```
S = 00 00
E = 8e 00
d = 8e
e = 2

        .----.----.                         .----.----.
    .-> | 8e | 00 |                         |    |    |
    |   '----'-|--'                         '----'-|--'
    |          v                                   v   
L = |         x8e                       T =       x1
    |          |                                   |   
    |          |                                   |   
    '----------'                            <------'   
```

step 2:

```
S = 00 8e 00
E = 78 8e 00
d = 78
e = 2

        .----.----.                         .----.
    .-> | 8e | 00 |                         |    |
    |   '-|--'-|--'                         '-|--'
    |     v    v                              v   
L = |    xf0  x8e                       T =  x2
    |     |    |                              |   
    |     v    |                              |   
    '---- + <--'                            <-'   
```

step 3:

```
S = 92 78 8e 00
E = a3 78 8e 00
d = 31
e = 2

        .----.----.                         .----.----.
    .-> | 8e | 00 |                         |    |    |
    |   '-|--'-|--'                         '----'-|--'
    |     v    v                                   v   
L = |    xf0  xec                       T =       x2
    |     |    |                                   |   
    |     v    |                                   |   
    '---- + <--'                            <------'   
```

step 4:

```
S = cb a3 78 8e 00
E = cb a3 78 8e 00
d = 00
e = 2

        .----.----.                         .----.----.----.
    .-> | 8e | 00 |                         |    |    |    |
    |   '-|--'-|--'                         '----'----'-|--'
    |     v    v                                        v   
L = |    xf0  xec                       T =            x2
    |     |    |                                        |   
    |     v    |                                        |   
    '---- + <--'                            <-----------'   
```

step 5:

```
S = 4b cb a3 78 8e 00
E = 86 cb a3 78 8e 00
d = cd
e = 4

        .----.----.----.----.               .----.----.----.----.
    .-> | a3 | 78 | 8e | 00 |               |    |    |    |    |
    |   '-|--'-|--'----'-|--'               '----'----'----'-|--'
    |     v    v         v                                   v   
L = |    xf0  xec       x87             T =                 x2
    |     |    |         |                                   |   
    |     v    v         |                                   |   
    '---- + <- + <-------'                  <----------------'   
```

step 6:

```
S = 80 86 cb a3 78 8e 00
E = 80 86 cb a3 78 8e 00
d = 00
e = 4

        .----.----.----.----.               .----.----.----.
    .-> | a3 | 78 | 8e | 00 |               |    |    |    |
    |   '-|--'-|--'----'-|--'               '-|--'-|--'-|--'
    |     v    v         v                    v    v    v    
L = |    xf0  xec       x87             T =  x7d  xc2  x67
    |     |    |         |                    |    |    | 
    |     v    v         |                    v    v    | 
    '---- + <- + <-------'                  < + <- + <--' 
```

step 7:

```
S = f9 80 86 cb a3 78 8e 00
E = 30 80 86 cb a3 78 8e 00
d = c9
e = 4

        .----.----.----.----.               .----.----.----.----.
    .-> | a3 | 78 | 8e | 00 |               |    |    |    |    |
    |   '-|--'-|--'-|--'-|--'               '----'-|--'-|--'-|--'
    |     v    v    v    v                         v    v    v    
L = |    xf0  x04  xdf  xea             T =       x7d  xc2  x67
    |     |    |    |    |                         |    |    | 
    |     v    v    v    |                         v    v    | 
    '---- + <- + <- + ---'                  <----- + <- + <--' 
```



TODO

#### Finding the error locations

Once we've figured out $\Lambda(x)$, we have everything we need to find
our error locations $X_j$.

The easiest thing to do is brute force, just plug in every location
$X_j=g^j$ in our codeword. If $\Lambda(X_j^{-1}) = 0$, we know $X_j$ is
the location of an error.

There are some other optimizations that can be applied here, mainly
[Chien's search][chiens-search], but as far as I can tell this is more
useful for hardware implementations and doesn't actually improve our
runtime when using Horner's method and GF(256) log tables.

#### Evaluating the errors

Once we've found our error locations $X_j$, solving for the error
magnitudes $Y_j$ is relatively straightforward. Kind of.

Recall the definition of our syndromes $S_i$:

$$
S_i = \sum_{j \in e} Y_j X_j^i
$$

With $e$ syndromes, this can be rewritten as a system of equations with
$e$ equations and $e$ unknowns, our error magnitudes $Y_j$, which we can
solve for:

$$  
\begin{bmatrix}  
S_0 \\  
S_1 \\  
\vdots \\  
S_{e-1} \\  
\end{bmatrix} =  
\begin{bmatrix}  
1 & 1 & \dots & 1\\  
X_{j_0} & X_{j_1} & \dots & X_{j_{e-1}}\\  
\vdots & \vdots & \ddots & \vdots \\  
X_{j_0}^{e-1} & X_{j_1}^{e-1} & \dots & X_{j_{e-1}}^{e-1}\\  
\end{bmatrix}  
\begin{bmatrix}  
Y_{j_0}\\  
Y_{j_1}\\  
\vdots \\  
Y_{j_{e-1}}\\  
\end{bmatrix}  
$$

#### Forney's algorithm

But again, solving this system of equations is easier said than done.

It turns out there's a really clever formula that can be used to solve
for $Y_j$ directly, called [Forney's algorithm][forneys-algorithm].

Forney's algorithm introduces two new polynomials. The error-evaluator
polynomial, $\Omega(x)$, defined like so:

$$
\Omega(x) = S(x) \Lambda(x) \bmod x^n
$$

Where $S(x)$ is the syndrome polynomial built using our syndromes $S_i$
as coefficients:

$$
S(x) = \sum_{i=0}^n S_i x^i
$$

And the [formal derivative][formal derivative] of the error-locator,
which we can calculate by terms:

$$
\Lambda'(x) = \sum_{i=1}^e i \Lambda_i x^{i-1}
$$

Note $i$ is not a field element, so multiplication by $i$ represents
normal repeated addition. And since addition is xor in our field, this
just cancels out every other term.

Combining these gives us a direct equation for an error-magnitude $Y_j$
given a known error-location $X_j$:

$$
Y_j = \frac{X_j \Omega(X_j^{-1})}{\Lambda'(X_j^)}
$$

#### WTF

Haha, I know right? Where did this equation come from? How does it work?
How did Forney even come up with this?

To be honest I don't know the answer to most of these questions, there's
very little documentation online about this formula comes from. But at
the very least we can prove that it works.

#### The error-evaluator polynomial

Let us start with the syndrome polynomial $S(x)$:

$$
S(x) = \sum_{i=0}^n S_i x^i
$$

Substituting the definition of $S_i$:

$$
S(x) = \sum_{i=0}^n \sum_{j \in e} Y_j X_j^i x^i
     = \sum_{j \in e} \left(Y_j \sum_{i=0}^n X_j^i x^i\right)
$$

The sum on the right side turns out to be a geometric series that we can
substitute in:

$$
S(x) = \sum_{j \in e} Y_j \frac{1 - X_j^n x^n}{1 - X_j x}
$$

If we then multiply with our error-locator polynomial $\Lambda(x)$:

$$
S(x)\Lambda(x) = \sum_{j \in e} \left(Y_j \frac{1 - X_j^n x^n}{1 - X_j x}\right) \cdot \prod_{k=0}^e \left(1 - X_k x\right)
               = \sum_{j \in e} \left(Y_j \left(1 - X_j^n x^n\right) \prod_{k \ne j} \left(1 - X_k x\right)\right)
$$

We see exactly one term in each summand (TODO summand??) cancel out.

At this point, if we plug in $X_j^{-1}$, this still evaluates to zero
thanks to the error-locator polynomial $\Lambda(x)$.

But if we expand the multiplication, something interesting happens:

$$
S(x)\Lambda(x) = \sum_{j \in e} \left(Y_j \prod_{k \ne j} \left(1 - X_k x\right)\right) - \sum_{j \in e} \left(Y_j X_j^n x^n \prod_{k \ne j} \left(1 - X_k x\right)\right)
$$

On the left side of the subtraction, all terms are at _most_ degree
$x^{e-1}$. On the gith side of the subtraction, all terms are at _least_
degree $x^n$.

Imagine how these contribute to the expanded form of the equation:

$$
S(x)\Lambda(x) = \overbrace{\Omega_0 + \dots + \Omega_{e-1} x^{e-1}}^{\sum_{j \in e} \left(Y_j \prod_{k \ne j} \left(1 - X_k x\right)\right)} + \overbrace{\Omega_n x^n + \dots + \Omega_{n+e-1} x^{n+e-1}}^{\sum_{j \in e} \left(Y_j X_j^n x^n \prod_{k \ne j} \left(1 - X_k x\right)\right)  }
$$

If we truncate this polynomial, $\bmod n$ in math land, we can
effectively delete part of the equation:

$$
S(x)\Lambda(x) \bmod x^n= \overbrace{\Omega_0 + \dots + \Omega_{e-1} x^{e-1}}^{\sum_{j \in e} \left(Y_j \prod_{k \ne j} \left(1 - X_k x\right)\right)}
$$

Giving us the equation for the error-evaluator polynomial $\Omega(x)$:

$$
\Omega(x) = S(x)\Lambda(x) \bmod x^n = \sum_{j \in e} \left(Y_j \prod_{k \ne j} \left(1 - X_k x\right)\right)
$$

What's really neat about the error-evaluator polynomial $\Omega(x)$ is
that $k \ne j$ condition.

The error-evaluator polynomial $\Omega(x)$ still contains a big chunk of
the error-locator polynomial $\Lambda(x)$. If we plug in an
error-location, $X_{j'}_{-1}$, _most_ of the terms evaluate to zero,
except the one where $j' \eq j$!

$$
\Omega(X_{j'}^{-1}) = \sum_{j \in e} \left(Y_j \prod_{k \ne j} \left(1 - X_k X_{j'}^{-1}\right)\right)
                    = Y_{j'} \prod_{k \ne j'} \left(1 - X_k X_{j'}^{-1}\right)
$$

And right there is our error-magnitude, $Y_{j'}$! Sure it's multiplied
with a bunch of gobbledygook, but it is there.

Fortunately that gobbledygook contains only our error-locators $X_k$,
which we know and in theory can remove with a bit of math.

#### The formal derivative of the error-locator polynomial

But Forney has another trick up his sleeve: The
[formal derivative][formal-derivative] of the error-locator polynomial,
$\Lambda'(x)$.

What the heck is a formal derivative?

Well we can't use normal derivatives in a finite-field like GF(256)
because they depend on the notion of a limit which depends on the field
being, well, not finite.

But derivatives are so useful mathematicians use them anyways.

Applying a formal derivative looks a lot like a normal derivative in
normal math:

$$
f(x) = f_0 + f_1 x + f_2 x^2 + \dots + f_i x^i
$$

$$
f'(x) = f_1 + 2 f_2 x + \dots + i f_i x^{i-1}
$$

Except $i$ here is not a finite-field element, so instead of doing
finite-field multiplication, we do normal repeated addition. And since
addition is xor in our field, this just cancels out every other term.

Quite a few properties of derivatives still hold in finite-fields. Of
particular interest to us is the product rule:

$$
\left(f(x) g(x)\right)' = f'(x) g(x) + f(x) g'(x)
$$

$$
\left(\prod_{i=0}^n f_i(x)\right)' = \sum_{i=0}^n \left(f_i'(x) \prod_{j \ne i} f_j(x)\right)
$$

Applying this to our error-locator polynomial $\Lambda(x):

$$
\Lambda(x) = 1 + \sum_{i=1}^e \Lambda_i x^i
$$

$$
\Lambda'(x) = \sum_{i=1}^e i \Lambda_i x^{i-1}
$$

Recall the other definition of our error-locator polynomial $\Lambda(x)$:

$$
\Lambda(x) = \prod_{j \in e} \left(1 - X_j x\right)
$$

$$
\Lambda'(x) = \left(\prod_{j \in e} \left(1 - X_j x\right)\right)'
$$

Applying the product rule:

$$
\Lambda'(x) = \sum_{j \in e} \left(X_j \prod_{k \ne j} \left(1 - X_k x\right)\right)
$$

Starting to look familiar?

Just like the error-evaluator polynomial $\Omega(x)$, plugging in an
error-location $X_{j'}^{-1}$ causes most of the terms to evaluate to
zero, except the one where $j' \eq j$, revealing $X_j$ times our
gobbledygook!

$$
\Lambda'(X_{j'}^{-1}) = \sum_{j \in e} \left(X_j \prod_{k \ne j} \left(1 - X_k X_{j'}^{-1}\right)\right)
                      = X_{j'} \prod_{k \ne j'} \left(1 - X_k X_{j'}^{-1}\right)
$$

If we divied $\Omega(X_{j'}^{-1})$ by $\Lambda'(X_{j'}^{-1})$, all that
gobbledygook cancels out, leaving us with a simply equation containing
$Y_{j'}$ and $X_{j'}$:

$$
\frac{\Omega(X_{j'}^{-1}}{\Lambda'(X_{j'}^{-1})} = \frac{Y_{j'} \prod_{k \ne j'} \left(1 - X_k X_{j'}^{-1}\right)}{X_{j'} \prod_{k \ne j'} \left(1 - X_k X_{j'}^{-1}\right)} = \frac{Y_{j'}}{X_{j'}}
$$

All that's left is to cancel out the $X_{j'}$ term to get our
error-magnitude $Y_{j'}$:

$$
\frac{X_{j'} \Omega(X_{j'}^{-1}}{\Lambda'(X_{j'}^{-1})} = \frac{X_{j'} Y_{j'} \prod_{k \ne j'} \left(1 - X_k X_{j'}^{-1}\right)}{X_{j'} \prod_{k \ne j'} \left(1 - X_k X_{j'}^{-1}\right)} = Y_{j'}
$$

#### Putting it all together

Once we've figured out the error-locator polynomial $\Lambda(x)$, the
error-evaluator polynomial $\Omega(x)$, and the derivative of the
error-locator polynomial $\Lambda'(x)$, we get to the fun part: Fixing
the errors!

For each location $j$ in the malformed codeword $C'(x)$, calculate the
error-location $X_j = g^j$ and plug its inverse $X_j^{-1}$ into the
error-locator $\Lambda(x)$. If $\Lambda(X_j^{-1}) = 0$ we've found the
location of an error!

To fix the error, plug the error-location $X_j$ and its inverse
$X_j^{-1}$ into Forney's formula to find the error-magnitude
$Y_j$: $Y_j = \frac{X_j \Omega(X_j^{-1})}{\Lambda'(X_j^{-1})}$. Xor
$Y_j$ into the codeword to fix this error!

Repeat for all errors in the malformed codeword $C'(x)$, and with any
luck we should find the original codeword $C(x)$!

$$
C(x) = C'(x) - \sum_{j \in e} Y_j x^j
$$

But we're not quite done. All of this math assumed we had
$e \le \frac{n}{2}$ errors. If we had more errors, it's possible we just
made things worse.

It's worth recalculating the syndromes after repairing errors to see if
we did ended up with a valid codeword:

$$
S_i = C(g^i) = 0
$$

If the syndromes are all zero, chances are high we successfully repaired
our codeword. Unless of course we had enough errors to end up
overcorrecting to another codeword...














vvvv TODO vvvv

Once we know our error-location $X_{j'}$ and error magnitude $Y_{j'}$, we
can xor our errored codeword $C'(x)$ with $Y_{j'}$ to remove this
specific error.

Repeat for all errors, and we should end up with our original codeword
$C(x)$!

vvvv TODO vvvv

The er

You can see the error-evaluator polynomial $\Omega(x)$ still contains a
big chunk of the error-locator polynomial $\Lambda(x)$. If we plug in an
error-locator, $X_j^{-1}$, _most_ of the terms evaluate to zero, except
one


vvvv TODO vvvv

The left side of the subtraction only contains terms up to $x^{e-1}$,
while the right side of the subtraction only contains terms  $x^n$




TODOTODOTODOTODOTODO




Let us start by multiply the syndrome polynomial $S(x)$ and the
error-locator polynomial $\Lambda(x)$:

$$
S(x) \Lambda(x) = \sum_{i=0}^n S_i x^i 
$$





To be honest I really don't know.

TODO





















vvvv use this? vvvv

Let's go back to to $\Lambda(X_j^{-1})$:

$$
\Lambda(X_j^{-1}) = 0
$$

We can multiply by $Y_jX_j^i$ for an arbitrary $i$ and the result will still be zero:

$$
X_j Y_j^{i}\Lambda(X_j^{-1}) = 0
$$

We can even add a bunch of these together and the result will still be zero:

$$
\sum_{j \in e} Y_j X_j^i \Lambda(X_j^{-1}) = 0
$$

And if we expand this mess, something interesting happens:


$$
\sum_{j \in e} Y_j X_j^i \Lambda(X_j^{-1}) = \sum_{j \in e} \left(Y_j X_j^i + \sum_{k=1}^e Y_j X_j^i \Lambda_k(X_j^{-1})^k\right)
$$

$$
= \sum_{j \in e} \left(Y_j X_j^i + \sum_{k=1}^e Y_j X_j^{i-k} \Lambda_k\right)
$$

$$
= \sum_{j \in e} Y_j X_j^i + \sum_{k=1}^e \left(\sum_{j \in e} Y_j X_j^{i-k}\right) \Lambda_k
$$

$$
= S_i + \sum_{k=1}^e S_{i-k} \Lambda_k
$$

But this is still be equal to zero, so we can rearrange this:

$$
S_i = \sum_{k=1}^e S_{i-k} \Lambda_k
$$

This gives us a system of equations with $e$ equations and $e$ unknowns:

$$
\begin{bmatrix}
S_{e} \\
S_{e+1} \\
\vdots \\
S_{n-1} \\
\end{bmatrix} =
\begin{bmatrix}
S_{e-1} & S_{e-2} & \dots & S_0\\
S_{e} & S_{e-1} & \dots & S_1\\
\vdots & \vdots & \ddots & \vdots \\
S_{n-2} & S_{n-3} & \dots & S_{e-1}\\
\end{bmatrix}
\begin{bmatrix}
\Lambda_1 \\
\Lambda_2 \\
\vdots \\
\Lambda_e \\
\end{bmatrix}
$$

Find via Berlekamp-Massey.







vvvv TODO vvvv

But of course we don't know $\Lambda(x)$ yet, we need to find it.

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

