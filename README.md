## ramrsbd

An example of a Reed-Solomon based error-correcting block device backed
by RAM.

```
corrupted:
'                 ..:::::7::::b:.. '       '  8     .:::....:::.96a:9:859.e:.dfbed5e 94ab8 4c:.4
      .     48 .:::::::::::::::::::..   '           :::::::::::::44ef696b 6d'6:e:79e5.748f.8fc'9
 8      '    .::::::::::':::':::::::::.            8::::::::::' e75a6ac555.f8cf8.b:9'ed57ad4a ec
           .:::::::::::::::::::::::::::. ' .  '  . ::::::::::   'c4ddf9fd:9.7e  ef 7a5c 7fa6'c4'
          :::::'::::::7:::7.:::::::::::::    ... :: :':::'::'   bb'c:da5a7 d6b.87d5b57b 9f7a: a:
         4::::::::::::::'::::::::::::::'' .. '::: '     '''     '7 aece5:de .68467c79fdd'49:8597
         ::::::::::::::::::::::'::::''.:::::: '        .  .     fb6b.6:'44f4'dcdf5e'.fac..:6fa8e
         :7:::::::::::::::::::::'' ..:::::''  '               ' dbaa4ba6e5a'c9 5a adb5e:7f8b554e
  8     8:::::::::::::::::::'' ..:::::'' ..               .    .' 'be4d.dd4b6c64:a5e'ba4:b578d :
   8 ..: ::::::::::::::''' ..:::::'' ..:::                      .a55f.cf:4':9.b8:44ea9c5bfd9c7e.
 8.::::'  :::::::::'' ..::::::'' ..:::::::              .       f8.'c7d.c9b:49a'48a:7 5c:9cc.6:b
 :::'4    '7::'' ...::::::''.5.::::::::::        .              edfe94e8:.b4ff':c655 5d6f7f5d:7'
::'         ...::::::6' ..:::::::::::::'       8  8             c88f.'96f8fbe5787bbad4f.bf:d9b
     9...:::::::'' ..::::::::::'::::.:'              4          ebe5eecda4fdee9b5:58:ee f:cafe::
'::::::::'''4.  ::::::::::::::::7::''    .  8                   ece:'b 6:7:a7.e5a'6a dfe'b967fe:
                  '':::::::::::'''                        ..   '99ade89:df5 7b7b e475ad:c98efaac
```

```
corrected:
                  ..:::::::::::...                  .:::....:::.96a:9:859.e:.dfbed5e 94ab   c:.4
               .:::::::::::::::::::..               :::::::::::::44ef696b 6d'6:e:79e5.748f.afc'9
             .::::::::::::::::::::::::.             ::::::::::' e75a6ac555.f8cf8.b:9'ad56ad5a ec
           .:::::::::::::::::::::::::::.         . ::::::::::   'c4dcf9fd:9.7e  ef 7a5c67fa6'c4'
          .::::::::::::::::::::::::::::::    ... :: :'::::::'   bb'c:da5a74d6b.87d5b57b 9f7a: a:
          :::::::::::::::::::::::::::::'' .. '::: '     '''     '7 aece5:de .68467c:9fdd'c9:8597
         :::::::::::::::::::::::::::''..::::: '                 fb6b.6:'44f4'dcdf5e'.fac..:6fa8e
         :::::::::::::::::::::::'' ..:::::''                    dbaa4ba6e5''c9 5b.adb5e:6f8b554e
         :::::::::::::::::::'' ..:::::'' .                      ' 'be4d.dd4b6c64:a5ea.a4:b578d :
     ..: ::::::::::::::''' ..:::::'' ..:::                      .a55f.cf:4':9.b8:446a8c5bfd9c7e.
  ..:::'  :::::::::'' ..::::::'' ..:::::::                      f8 'c6d.c9b:49a'48a:7 5c79cc.6:b
 :::'     ':::'' ...::::::''...::::::::::                       edfe94e8:.b4ff':c65585d'f7b5d:7'
::'         ...::::::'' ..:::::::::::::'                        c88f:'96f8fbc5787bbad4f.bf:d9b8
     ....:::::::'' ..:::::::::::::::::'                         ebe5eecda4fdee8b5:58:ee e:cafe::
'::::::::'''    :::::::::::::::::::''                           ece:'b 6:7:af9e5a'6' dfe'b967fe:
                  '':::::::::::'''                              99adf89:df5 7b7b e475ad:c98efaac
```

Reed-Solomon codes are sort of the big brother to CRCs. Operating on
bytes instead of bits, they are both flexible and powerful, capable of
detecting and correcting a configurable number of byte errors
efficiently.

Assuming `ecc_size` bytes of ECC, ramrsbd can correct `floor(ecc_size/2)`
byte errors in up to 255 byte codewords (yes, 255 bytes, not 256 :/).

The main tradeoff is they are quite a bit more complex mathematically,
which adds some code, RAM, and brain cost.

A quick comparison of current ram-ecc-bds:

|            | code   | tables | stack | buffers  | runtime                  |
|:-----------|-------:|-------:|------:|---------:|-------------------------:|
| ramcrc32bd |  940 B |   64 B |  88 B |      0 B |      $O\left(n^e\right)$ |
| ramrsbd    | 1506 B |  512 B | 128 B | n + 4e B | $O\left(ne + e^2\right)$ |

See also:

- [littlefs][littlefs]
- [ramcrc32bd][ramcrc32bd]

## RAM?

Right now, [littlefs's][littlefs] block device API is limited in terms of
composability. While it would be great to fix this on a major API change,
in the meantime, a RAM-backed block device provides a simple example of
error-correction that users may be able to reimplement in their own
block devices.

## Testing

Testing is a bit jank right now, relying on littlefs's test runner:

``` bash
$ git clone https://github.com/littlefs-project/littlefs -b v2.9.3 --depth 1
$ make test -j
```

## Words of warning

Before we get into how the algorithm works, a couple words of warning:

1. I'm not a mathematician! Some of the definitions here are a bit
   handwavey, and I'm skipping over the history of [BCH][w-bch] codes,
   [PGZ][w-pgz], [Euclidean methods][w-euclidean], etc. I'd encourage you
   to also explore [Wikipedia][w-rs] and other relevant articles to learn
   more.

   My goal is to explain, to the best of my (limited) knowledge, how to
   implement Reed-Solomon codes, and how/why they work.

2. The following math relies heavily on [finite-fields][w-gf] (sometimes
   called Galois-fields) and the related theory.

   If you're not familiar with finite-fields, they are an abstraction we
   can make over finite numbers (bytes for [GF(256)][w-gf256], bits for
   [GF(2)][w-gf2]) that let us do most of math without worrying about
   pesky things like integer overflow.

   But there's not enough space here to fully explain how they work, so
   I'd suggest reading some of the above articles first.

3. This is some pretty intense math! Reed-Solomon has evolved over
   several decades and many PhDs. Just look how many names are involved
   in this thing. So don't get discouraged if it feels impenetrable.

   Feel free to take a break to remind yourself that math is not real and
   can not hurt you.

   I'd also encourage you to use GitHub's table-of-contents to jump
   around and keep track of where you are.

## How it works

Like CRCs, Reed-Solomon codes are implemented by concatenating the
message with the remainder after division by a predetermined "generator
polynomial", giving us a [systematic code][w-systematic-code].

However, two important differences:

1. Instead of using a binary polynomial in [GF(2)][w-gf2], we use a
   polynomial in a higher-order [finite-field][w-gf], usually
   [GF(256)][w-gf256] because operating on bytes is convenient.

2. We intentionally construct the polynomial to tell us information about
   any errors that may occur.

   Specifically, we constrain our polynomial (and implicitly our
   codewords) to evaluate to zero at $n$ fixed points. As long as we have
   $e \le \frac{n}{2}$ errors, we can solve for errors using these fixed
   points.

   Note this isn't really possible with CRCs in GF(2), because the only
   non-zero binary number is, well, 1. GF(256) has 255 non-zero elements,
   which we'll see becomes quite important.

### Constructing a generator polynomial

Ok, first step, constructing a generator polynomial.

If we want to correct $e$ byte-errors, we will need $n = 2e$ fixed
points. We can construct a generator polynomial $P(x)$ with $n$ fixed
points at $g^i$ where $i < n$ like so:

<p align="center">
<img
    alt="P(x) = \prod_{i=0}^{n-1} \left(x - g^i\right) = \left(x-1\right)\left(x-g\right)\cdots\left(x-g^{n-1}\right)"
    src="https://latex.codecogs.com/svg.image?P%28x%29%20%3d%20%5cprod_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20%5cleft%28x%20%2d%20g%5ei%5cright%29%20%3d%20%5cleft%28x%2d%31%5cright%29%5cleft%28x%2dg%5cright%29%5ccdots%5cleft%28x%2dg%5e%7bn%2d%31%7d%5cright%29"
>
</p>

We could choose any arbitrary set of fixed points, but usually we choose
$g^i$ where $g$ is a [generator][w-generator] in GF(256), since it
provides a convenient mapping of integers to unique non-zero elements in
GF(256).

Note that for any fixed point $g^i$:

<p align="center">
<img
    alt="g^i - g^i = 0"
    src="https://latex.codecogs.com/svg.image?g%5ei%20%2d%20g%5ei%20%3d%20%30"
>
</p>

And since multiplying anything by zero is zero, this will make our entire
product zero. So for any fixed point $g^i$, $P(g^i)$ should evaluate to
zero:

<p align="center">
<img
    alt="P(g^i) = 0"
    src="https://latex.codecogs.com/svg.image?P%28g%5ei%29%20%3d%20%30"
>
</p>

This gets real nifty when you look at the definition of our Reed-Solomon
code for codeword $C(x)$ given a message $M(x)$:

<p align="center">
<img
    alt="C(x) = M(x) x^n - \left(M(x) x^n \bmod P(x)\right)"
    src="https://latex.codecogs.com/svg.image?C%28x%29%20%3d%20M%28x%29%20x%5en%20%2d%20%5cleft%28M%28x%29%20x%5en%20%5cbmod%20P%28x%29%5cright%29"
>
</p>

As is true with normal math, subtracting the remainder after division
gives us a polynomial that is a multiple of $P(x)$. And since multiplying
anything by zero is zero, for any fixed point $g^i$, $C(g^i)$ should also
evaluate to zero:

<p align="center">
<img
    alt="C(g^i) = 0"
    src="https://latex.codecogs.com/svg.image?C%28g%5ei%29%20%3d%20%30"
>
</p>

#### Modeling errors

Ok, but what if there are errors?

We can think of introducing errors as adding an error polynomial $E(x)$
to our original codeword, where $E(x)$ contains up to $e$ non-zero terms:

<p align="center">
<img
    alt="C'(x) = C(x) + E(x)"
    src="https://latex.codecogs.com/svg.image?C%27%28x%29%20%3d%20C%28x%29%20%2b%20E%28x%29"
>
</p>

<p align="center">
<img
    alt="E(x) = \sum_{j \in E} E_j x^j = E_{j_0} x^{j_0} + E_{j_1} x^{j_1} + \cdots + E_{j_{e-1}} x^{j_{e-1}}"
    src="https://latex.codecogs.com/svg.image?E%28x%29%20%3d%20%5csum_%7bj%20%5cin%20E%7d%20E_j%20x%5ej%20%3d%20E_%7bj_%30%7d%20x%5e%7bj_%30%7d%20%2b%20E_%7bj_%31%7d%20x%5e%7bj_%31%7d%20%2b%20%5ccdots%20%2b%20E_%7bj_%7be%2d%31%7d%7d%20x%5e%7bj_%7be%2d%31%7d%7d"
>
</p>

Check out what happens if we plug in our fixed point $g^i$:

<p align="center">
<img
    alt="\begin{aligned} C'(g^i) &= C(g^i) + E(g^i) \\ &= 0 + E(g^i) \\ &= E(g^i) \end{aligned}"
    src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20C%27%28g%5ei%29%20%26%3d%20C%28g%5ei%29%20%2b%20E%28g%5ei%29%20%5c%5c%20%26%3d%20%30%20%2b%20E%28g%5ei%29%20%5c%5c%20%26%3d%20E%28g%5ei%29%20%5cend%7baligned%7d"
>
</p>

The original codeword drops out! Leaving us with an equation defined only
by the error polynomial.

We call these evaluations our "syndromes" $S_i$, since they tell us
information about the errors in our codeword:

<p align="center">
<img
    alt="S_i = C'(g^i) = E(g^i) = \sum_{j \in E} E_j g^{ji}"
    src="https://latex.codecogs.com/svg.image?S_i%20%3d%20C%27%28g%5ei%29%20%3d%20E%28g%5ei%29%20%3d%20%5csum_%7bj%20%5cin%20E%7d%20E_j%20g%5e%7bji%7d"
>
</p>

We usually refer to the unknowns in this equation as the
"error-locations" $X_j = g^j$, and the "error-magnitudes" $Y_j = E_j$:

<p align="center">
<img
    alt="S_i = C'(g^i) = E(g^i) = \sum_{j \in E} Y_j X_j^i"
    src="https://latex.codecogs.com/svg.image?S_i%20%3d%20C%27%28g%5ei%29%20%3d%20E%28g%5ei%29%20%3d%20%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5ei"
>
</p>

Note that finding $X_j$ also gives us $j$, since $j = \log_g X_j$. We
usually write it this way just to avoid adding a bunch of $g^j$
everywhere.

If we can figure out both the error-locations and error-magnitudes, we
have enough information to reconstruct our original codeword:

<p align="center">
<img
    alt="C(x) = C'(x) - \sum_{j \in E} Y_j x^j"
    src="https://latex.codecogs.com/svg.image?C%28x%29%20%3d%20C%27%28x%29%20%2d%20%5csum_%7bj%20%5cin%20E%7d%20Y_j%20x%5ej"
>
</p>

### Finding the error locations

Ok, let's say we received a codeword $C'(x)$ with $e$ errors. Evaluating
at our fixed points $g^i$, where $i < n$ and $n \ge 2e$, gives us our
syndromes $S_i$:

<p align="center">
<img
    alt="S_i = C'(g^i) = E(g^i) = \sum_{j \in E} Y_j X_j^i"
    src="https://latex.codecogs.com/svg.image?S_i%20%3d%20C%27%28g%5ei%29%20%3d%20E%28g%5ei%29%20%3d%20%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5ei"
>
</p>

The next step is figuring out the error-locations $X_j$.

To help with this, we introduce a very special polynomial, the
"error-locator polynomial" $\Lambda(x)$:

<p align="center">
<img
    alt="\Lambda(x) = \prod_{j \in E} \left(1 - X_j x\right) = \left(1 - X_{j_0} x\right)\left(1 - X_{j_1} x\right)\cdots\left(1 - X_{j_{e-1}} x\right)"
    src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%5cprod_%7bj%20%5cin%20E%7d%20%5cleft%28%31%20%2d%20X_j%20x%5cright%29%20%3d%20%5cleft%28%31%20%2d%20X_%7bj_%30%7d%20x%5cright%29%5cleft%28%31%20%2d%20X_%7bj_%31%7d%20x%5cright%29%5ccdots%5cleft%28%31%20%2d%20X_%7bj_%7be%2d%31%7d%7d%20x%5cright%29"
>
</p>

This polynomial has some rather useful properties:

1. For any error-location $X_j$, $\Lambda(X_j^{-1})$ evaluates to zero:

   <p align="center">
   <img
       alt="\Lambda(X^{-1}) = 0"
       src="https://latex.codecogs.com/svg.image?%5cLambda%28X%5e%7b%2d%31%7d%29%20%3d%20%30"
   >
   </p>

   This is for similar reasons why $P(g^i) = 0$. For any error-location
   $X_j$:

   <p align="center">
   <img
       alt="\begin{aligned} 1 - X_j X_j^{-1} &= 1 - 1 \\ &= 0 \end{aligned}"
       src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20%31%20%2d%20X_j%20X_j%5e%7b%2d%31%7d%20%26%3d%20%31%20%2d%20%31%20%5c%5c%20%26%3d%20%30%20%5cend%7baligned%7d"
   >
   </p>

   And since multiplying anything by zero is zero, the product reduces to
   zero.

2. $\Lambda(0)$ evaluates to one:

   <p align="center">
   <img
       alt="\Lambda(0) = 1"
       src="https://latex.codecogs.com/svg.image?%5cLambda%28%30%29%20%3d%20%31"
   >
   </p>

   This can be seen by plugging in 0:

   <p align="center">
   <img
       alt="\begin{aligned} \Lambda(0) &= \prod_{j \in E} \left(1 - X_j \cdot 0\right) \\ &= \prod_{j \in E} 1 \\ &= 1 \end{aligned}"
       src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20%5cLambda%28%30%29%20%26%3d%20%5cprod_%7bj%20%5cin%20E%7d%20%5cleft%28%31%20%2d%20X_j%20%5ccdot%20%30%5cright%29%20%5c%5c%20%26%3d%20%5cprod_%7bj%20%5cin%20E%7d%20%31%20%5c%5c%20%26%3d%20%31%20%5cend%7baligned%7d"
   >
   </p>

   This prevents trivial solutions and is what makes $\Lambda(x)$ useful.

What's _really_ interesting is that these two properties allow us to
solve for $\Lambda(x)$ with only our syndromes $S_i$.

We know $\Lambda(x)$ has $e$ roots, which means we can expand it into a
polynomial with $e+1$ terms. We also know that $\Lambda(0) = 1$, so the
constant term must be 1. Giving the coefficients of this expanded
polynomial the arbitrary names
$\Lambda_1, \Lambda_2, \cdots, \Lambda_e$, we find another definition for
$\Lambda(x)$:

<p align="center">
<img
    alt="\Lambda(x) = 1 + \sum_{k=1}^e \Lambda_k x^k = 1 + \Lambda_1 x + \Lambda_2 x^2 + \cdots + \Lambda_e x^e"
    src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%31%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20x%5ek%20%3d%20%31%20%2b%20%5cLambda_%31%20x%20%2b%20%5cLambda_%32%20x%5e%32%20%2b%20%5ccdots%20%2b%20%5cLambda_e%20x%5ee"
>
</p>

Note this doesn't actually change our error-locator $\Lambda(x)$, it
still has all of its original properties. For example, if we plug in
$X_j^{-1}$ it should still evaluate to zero:

<p align="center">
<img
    alt="\Lambda(X_j^{-1}) = 1 + \sum_{k=1}^e \Lambda_k X_j^{-k} = 0"
    src="https://latex.codecogs.com/svg.image?%5cLambda%28X_j%5e%7b%2d%31%7d%29%20%3d%20%31%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20X_j%5e%7b%2dk%7d%20%3d%20%30"
>
</p>

And since multiplying anything by zero is zero, we can multiply this by,
say, $Y_j X_j^i$, and the result should still be zero:

<p align="center">
<img
    alt="Y_j X_j^i \Lambda(X_j^{-1}) = Y_j X_j^i + \sum_{k=1}^e \Lambda_k Y_j X_j^{i-k} = 0"
    src="https://latex.codecogs.com/svg.image?Y_j%20X_j%5ei%20%5cLambda%28X_j%5e%7b%2d%31%7d%29%20%3d%20Y_j%20X_j%5ei%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20Y_j%20X_j%5e%7bi%2dk%7d%20%3d%20%30"
>
</p>

We can even add a bunch of these together and the result should still be
zero:

<p align="center">
<img
    alt="\sum_{j \in E} Y_j X_j^i \Lambda(X_j^{-1}) = \sum_{j \in E} \left(Y_j X_j^i + \sum_{k=1}^e \Lambda_k Y_j X_j^{i-k}\right) = 0"
    src="https://latex.codecogs.com/svg.image?%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5ei%20%5cLambda%28X_j%5e%7b%2d%31%7d%29%20%3d%20%5csum_%7bj%20%5cin%20E%7d%20%5cleft%28Y_j%20X_j%5ei%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20Y_j%20X_j%5e%7bi%2dk%7d%5cright%29%20%3d%20%30"
>
</p>

Wait a second...

<p align="center">
<img
    alt="\sum_{j \in E} Y_j X_j^i \Lambda(X_j^{-1}) = \left(\sum_{j \in E} Y_j X_j^i\right) + \sum_{k=1}^e \Lambda_k \left(\sum_{j \in E} Y_j X_j^{i-k}\right) = 0"
    src="https://latex.codecogs.com/svg.image?%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5ei%20%5cLambda%28X_j%5e%7b%2d%31%7d%29%20%3d%20%5cleft%28%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5ei%5cright%29%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20%5cleft%28%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5e%7bi%2dk%7d%5cright%29%20%3d%20%30"
>
</p>

Aren't these our syndromes? $S_i = \sum_{j \in E} Y_j X_j^i$?

<p align="center">
<img
    alt="\sum_{j \in E} Y_j X_j^i \Lambda(X_j^{-1}) = S_i + \sum_{k=1}^e \Lambda_k S_{i-k} = 0"
    src="https://latex.codecogs.com/svg.image?%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5ei%20%5cLambda%28X_j%5e%7b%2d%31%7d%29%20%3d%20S_i%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20S_%7bi%2dk%7d%20%3d%20%30"
>
</p>

They are! We can rearrange this into an equation for $S_i$ using only our
coefficients $\Lambda_k$ and $e$ previously seen syndromes
$S_{i-1}, S_{i-2}, \cdots, S_{i-e}$:

<p align="center">
<img
    alt="S_i = \sum_{k=1}^e \Lambda_k S_{i-k} = \Lambda_1 S_{i-1} + \Lambda_2 S_{i-2} + \cdots + \Lambda_e S_{i-e}"
    src="https://latex.codecogs.com/svg.image?S_i%20%3d%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20S_%7bi%2dk%7d%20%3d%20%5cLambda_%31%20S_%7bi%2d%31%7d%20%2b%20%5cLambda_%32%20S_%7bi%2d%32%7d%20%2b%20%5ccdots%20%2b%20%5cLambda_e%20S_%7bi%2de%7d"
>
</p>

If we repeat this $e$ times, for syndromes
$S_e, S_{e+1}, \cdots, S_{n-1}$, we end up with $e$ equations and
$e$ unknowns. A system that is, in theory, solvable:

<p align="center">
<img
    alt="\begin{bmatrix} S_{e} \\ S_{e+1} \\ \vdots \\ S_{n-1} \end{bmatrix} = \begin{bmatrix} S_{e-1} & S_{e-2} & \cdots & S_0 \\ S_{e} & S_{e-1} & \cdots & S_1 \\ \vdots & \vdots & \ddots & \vdots \\ S_{n-2} & S_{n-3} & \cdots & S_{e-1} \end{bmatrix} \begin{bmatrix} \Lambda_1 \\ \Lambda_2 \\ \vdots \\ \Lambda_e \end{bmatrix}"
    src="https://latex.codecogs.com/svg.image?%5cbegin%7bbmatrix%7d%20S_%7be%7d%20%5c%5c%20S_%7be%2b%31%7d%20%5c%5c%20%5cvdots%20%5c%5c%20S_%7bn%2d%31%7d%20%5cend%7bbmatrix%7d%20%3d%20%5cbegin%7bbmatrix%7d%20S_%7be%2d%31%7d%20%26%20S_%7be%2d%32%7d%20%26%20%5ccdots%20%26%20S_%30%20%5c%5c%20S_%7be%7d%20%26%20S_%7be%2d%31%7d%20%26%20%5ccdots%20%26%20S_%31%20%5c%5c%20%5cvdots%20%26%20%5cvdots%20%26%20%5cddots%20%26%20%5cvdots%20%5c%5c%20S_%7bn%2d%32%7d%20%26%20S_%7bn%2d%33%7d%20%26%20%5ccdots%20%26%20S_%7be%2d%31%7d%20%5cend%7bbmatrix%7d%20%5cbegin%7bbmatrix%7d%20%5cLambda_%31%20%5c%5c%20%5cLambda_%32%20%5c%5c%20%5cvdots%20%5c%5c%20%5cLambda_e%20%5cend%7bbmatrix%7d"
>
</p>

This is where the $n=2e$ requirement comes from, and why we need $n=2e$
syndromes to solve for $e$ errors at unknown locations.

#### Berlekamp-Massey

Ok that's the theory, but solving this system of equations efficiently is
still quite difficult.

Enter [Berlekamp-Massey][w-bm].

A key observation by Massey is that solving for $\Lambda(x)$ is
equivalent to constructing an [LFSR][w-lfsr] that generates the sequence
$S_e, S_{e+1}, \dots, S_{n-1}$ given the initial state
$S_0, S_1, \dots, S_{e-1}$:

```
.---- + <- + <- + <- + <--- ... --- + <--.
|     ^    ^    ^    ^              ^    |
|    *Λ1  *Λ2  *Λ3  *Λ4     ...   *Λe-1 *Λe
|     ^    ^    ^    ^              ^    ^
|   .-|--.-|--.-|--.-|--.--     --.-|--.-|--.
'-> |Se-1|Se-2|Se-3|Se-4|   ...   | S1 | S0 | -> Sn-1 Sn-2 ... S2+3 Se+2 Se+1 Se Se-1 Se-2 ... S3 S2 S1 S0
    '----'----'----'----'--     --'----'----'
```

Pretty wild huh.

We can describe such an LFSR with a [recurrence relation][w-recurrence-relation]
that might look a bit familiar:

<p align="center">
<img
    alt="L(i) = s_i = \sum_{k=1}^{|L|} L_k s_{i-k} = L_1 s_{i-1} + L_2 s_{i-2} + \cdots + L_{|L|} s_{i-|L|}"
    src="https://latex.codecogs.com/svg.image?L%28i%29%20%3d%20s_i%20%3d%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20s_%7bi%2dk%7d%20%3d%20L_%31%20s_%7bi%2d%31%7d%20%2b%20L_%32%20s_%7bi%2d%32%7d%20%2b%20%5ccdots%20%2b%20L_%7b%7cL%7c%7d%20s_%7bi%2d%7cL%7c%7d"
>
</p>

Berlekamp-Massey relies on two key observations:

1. If an LFSR $L(i)$ of size $|L|$ generates the sequence
   $s_0, s_1, \dots, s_{n-1}$, but fails to generate the sequence
   $s_0, s_1, \dots, s_{n-1}, s_n$, than an LFSR $L'(i)$ that _does_
   generate the sequence must have a size of at least:

   <p align="center">
   <img
       alt="|L'| \ge n+1-|L|"
       src="https://latex.codecogs.com/svg.image?%7cL%27%7c%20%5cge%20n%2b%31%2d%7cL%7c"
   >
   </p>

   Massey's proof of this is very math heavy.

   Consider the equation for our LFSR $L(i)$ at $n$:

   <p align="center">
   <img
       alt="L(n) = \sum_{k=1}^{|L|} L_k s_{n-k} \ne s_n"
       src="https://latex.codecogs.com/svg.image?L%28n%29%20%3d%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20s_%7bn%2dk%7d%20%5cne%20s_n"
   >
   </p>

   If we have another LFSR $L'(i)$ that generates
   $s_{n-|L|}, s_{n-|L|+1}, \cdots, s_{n-1}$, we can substitute it in for
   $s_{n-k}$:

   <p align="center">
   <img
       alt="\begin{aligned} L(n) &= \sum_{k=1}^{|L|} L_k s_{n-k} \\ &= \sum_{k=1}^{|L|} L_k L'(n-k) \\ &= \sum_{k=1}^{|L|} L_k \sum_{l=1}^{|L'|} L'_l s_{n-k-l} \\ \end{aligned}"
       src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20L%28n%29%20%26%3d%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20s_%7bn%2dk%7d%20%5c%5c%20%26%3d%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20L%27%28n%2dk%29%20%5c%5c%20%26%3d%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20%5csum_%7bl%3d%31%7d%5e%7b%7cL%27%7c%7d%20L%27_l%20s_%7bn%2dk%2dl%7d%20%5c%5c%20%5cend%7baligned%7d"
   >
   </p>

   Multiplication is distributive, so we can move our summations around:

   <p align="center">
   <img
       alt="L(n) = \sum_{l=1}^{|L'|} L'_l \sum_{k=1}^{|L|} L_k s_{n-l-k}"
       src="https://latex.codecogs.com/svg.image?L%28n%29%20%3d%20%5csum_%7bl%3d%31%7d%5e%7b%7cL%27%7c%7d%20L%27_l%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20s_%7bn%2dl%2dk%7d"
   >
   </p>

   And note that right summation looks a lot like $L(i)$. If $L(i)$
   generates $s_{n-|L'|}, s_{n-|L'|+1}, \cdots, s_{n-1}$, we can replace
   it with $s_{n-k'}$:

   <p align="center">
   <img
       alt="\begin{aligned} L(n) &= \sum_{l=1}^{|L'|} L'_l \sum_{k=1}^{|L|} L_k s_{n-l-k} \\ &= \sum_{l=1}^{|L'|} L'_l L(n-l) \\ &= \sum_{l=1}^{|L'|} L'_l s_{n-l} \end{aligned}"
       src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20L%28n%29%20%26%3d%20%5csum_%7bl%3d%31%7d%5e%7b%7cL%27%7c%7d%20L%27_l%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20s_%7bn%2dl%2dk%7d%20%5c%5c%20%26%3d%20%5csum_%7bl%3d%31%7d%5e%7b%7cL%27%7c%7d%20L%27_l%20L%28n%2dl%29%20%5c%5c%20%26%3d%20%5csum_%7bl%3d%31%7d%5e%7b%7cL%27%7c%7d%20L%27_l%20s_%7bn%2dl%7d%20%5cend%7baligned%7d"
   >
   </p>
   
   Oh hey! That's the definition of $L'(i)$:

   <p align="center">
   <img
       alt="L(n) = L'(n) = s_n"
       src="https://latex.codecogs.com/svg.image?L%28n%29%20%3d%20L%27%28n%29%20%3d%20s_n"
   >
   </p>

   So if $L'(i)$ generates $s_n$, $L(i)$ must also generate $s_n$.

   The only way to make $L'(i)$ generate a different $s_n$ would be to
   make $|L'| \ge n+1-|L|$ so that $L(i)$ can no longer generate
   $s_{n-|L'|}, s_{n-|L'|+1}, \cdots, s_{n-1}$.

2. Once we've found the best LFSR $L(i)$ for a given size $|L|$, its
   definition provides an optimal strategy for changing only the last
   element of the generated sequence.

   This is assuming $L(i)$ failed of course. If $L(i)$ generated the
   whole sequence our algorithm is done!

   If $L(i)$ failed, we assume it correctly generated
   $s_0, s_1, \cdots, s_{n-1}$, but failed at $s_n$. We call the
   difference from the expected symbol the discrepancy $d$:

   <p align="center">
   <img
       alt="L(i) = \sum_{k=1}^{|L|} L_k s_{i-k} = \begin{cases} s_i & i = |L|, |L|+1, \cdots, n-1 \\ s_i+d & i = n \end{cases}"
       src="https://latex.codecogs.com/svg.image?L%28i%29%20%3d%20%5csum_%7bk%3d%31%7d%5e%7b%7cL%7c%7d%20L_k%20s_%7bi%2dk%7d%20%3d%20%5cbegin%7bcases%7d%20s_i%20%26%20i%20%3d%20%7cL%7c%2c%20%7cL%7c%2b%31%2c%20%5ccdots%2c%20n%2d%31%20%5c%5c%20s_i%2bd%20%26%20i%20%3d%20n%20%5cend%7bcases%7d"
   >
   </p>

   If we know $s_i$ (which requires a larger LFSR), we can rearrange this
   to be a bit more useful. We call this our connection polynomial
   $C(i)$:

   <p align="center">
   <img
       alt="C(i) = d^{-1} \cdot \left(L(i) - s_i\right) = \begin{cases} 0 & i = |L|, |L|+1,\cdots,n-1 \\ 1 & i = n \end{cases}"
       src="https://latex.codecogs.com/svg.image?C%28i%29%20%3d%20d%5e%7b%2d%31%7d%20%5ccdot%20%5cleft%28L%28i%29%20%2d%20s_i%5cright%29%20%3d%20%5cbegin%7bcases%7d%20%30%20%26%20i%20%3d%20%7cL%7c%2c%20%7cL%7c%2b%31%2c%5ccdots%2cn%2d%31%20%5c%5c%20%31%20%26%20i%20%3d%20n%20%5cend%7bcases%7d"
   >
   </p>

   Now, if we have a larger LFSR $L'(i)$ with size $|L'| \gt |L|$ and we
   want to change only the symbol $s'_n$ by $d'$, we can add
   $d' \cdot C(i)$, and only $s'_n$ will be affected:

   <p align="center">
   <img
       alt="L'(i) + d' \cdot C(i) = \begin{cases} s'_i & i = |L'|,|L'|+1,\cdots,n-1 \\ s'_i + d' & i = n \end{cases}"
       src="https://latex.codecogs.com/svg.image?L%27%28i%29%20%2b%20d%27%20%5ccdot%20C%28i%29%20%3d%20%5cbegin%7bcases%7d%20s%27_i%20%26%20i%20%3d%20%7cL%27%7c%2c%7cL%27%7c%2b%31%2c%5ccdots%2cn%2d%31%20%5c%5c%20s%27_i%20%2b%20d%27%20%26%20i%20%3d%20n%20%5cend%7bcases%7d"
   >
   </p>

If you can wrap your head around those two observations, you have
understood most of Berlekamp-Massey.

The actual algorithm itself is relatively simple:  
  
1. Using the current LFSR $L(i)$, generate the next symbol and calculate
   the discrepancy $d$ between it and the expected symbol $s_n$:

   <p align="center">
   <img
       alt="d = L(n) - s_n"
       src="https://latex.codecogs.com/svg.image?d%20%3d%20L%28n%29%20%2d%20s_n"
   >
   </p>
  
2. If $d=0$, great! Move on to the next symbol.  
  
3. If $d \ne 0$, we need to tweak our LFSR:

   1. First check if our LFSR is big enough. If $n \ge 2|L|$, we need a
      bigger LFSR:

      <p align="center">
      <img
          alt="|L'| = n+1-|L|"
          src="https://latex.codecogs.com/svg.image?%7cL%27%7c%20%3d%20n%2b%31%2d%7cL%7c"
      >
      </p>

      If we're changing the size, save the best LFSR at the current size
      for future tweaks:

      <p align="center">
      <img
          alt="C'(i) = d^{-1} \cdot \left(L(i) - s_i\right)"
          src="https://latex.codecogs.com/svg.image?C%27%28i%29%20%3d%20d%5e%7b%2d%31%7d%20%5ccdot%20%5cleft%28L%28i%29%20%2d%20s_i%5cright%29"
      >
      </p>

   2. Now we can fix the LFSR by adding our last $C(i)$ (not $C'(i)$!),
      shifting and scaling so only $s_n$ is affected:

      <p align="center">
      <img
          alt="L'(i) = L(i) + d \cdot C(i-(n-m))"
          src="https://latex.codecogs.com/svg.image?L%27%28i%29%20%3d%20L%28i%29%20%2b%20d%20%5ccdot%20C%28i%2d%28n%2dm%29%29"
      >
      </p>

      Where $m$ is the value of $n$ when we saved the last $C(i)$. If we
      shift $C(i)$ every step of the algorithm, we usually don't need to
      track $m$ explicitly.

This is all implemented in `ramrsbd_find_λ`.

#### Solving binary LFSRs for fun

Taking a step away from [GF(256)][w-gf256] for a moment, let's look at a
simpler LFSR in [GF(2)][w-gf2], aka binary.

Consider this binary sequence generated by a minimal LFSR that I know and
you don't :)

```
1 1 0 0 1 1 1 1
```

Can you figure out the original LFSR?

<details>
<summary>
Click here to solve with Berlekamp-Massey
</summary>

---

Using Berlekamp-Massey:

To start, let's assume our LFSR is an empty LFSR that just spits out a
stream of zeros. Not the most creative LFSR, but we have to start
somewhere!

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

---

</details>

#### bm-lfsr-solver.py

In case you want to play around with this algorithm more (and for my own
experiments), I've ported this LFSR solver to Python in
[bm-lfsr-solver.py][bm-lfsr-solver.py]. Feel free to try your own binary
sequences:

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

I've also implemented a similar script,
[bm-lfsr256-solver.py][bm-lfsr256-solver.py], for full GF(256) LFSRs,
though it's a bit harder for follow unless you can do full GF(256)
multiplications in your head:

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

Is Berlekamp-Massey a good compression algorithm? Probably not.

#### Locating the errors

Coming back to Reed-Solomon, thanks to Berlekamp-Massey we can solve the
following recurrence for the terms $\Lambda_k$ given at least $n \ge 2e$
syndromes $S_i$:

<p align="center">
<img
    alt="\Lambda(i) = S_i = \sum_{k=1}^e \Lambda_k S_{i-k} = \Lambda_1 S_{i-1} + \Lambda_2 S_{i-2} + \cdots + \Lambda_e S_{i-e}"
    src="https://latex.codecogs.com/svg.image?%5cLambda%28i%29%20%3d%20S_i%20%3d%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20S_%7bi%2dk%7d%20%3d%20%5cLambda_%31%20S_%7bi%2d%31%7d%20%2b%20%5cLambda_%32%20S_%7bi%2d%32%7d%20%2b%20%5ccdots%20%2b%20%5cLambda_e%20S_%7bi%2de%7d"
>
</p>

These terms define our error-locator polynomial, which we can use to
find the locations of errors:

<p align="center">
<img
    alt="\Lambda(x) = 1 + \sum_{k=1}^e \Lambda_k x^k = \prod_{j \in E} \left(1 - X_j x\right)"
    src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%31%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20x%5ek%20%3d%20%5cprod_%7bj%20%5cin%20E%7d%20%5cleft%28%31%20%2d%20X_j%20x%5cright%29"
>
</p>

All we have left to do is figure out where $\Lambda(X_j^{-1})=0$.

The easiest way to do this is just brute force: Just plug in every
location $X_j=g^j$ in our codeword, and if $\Lambda(X_j^{-1}) = 0$, we
know $X_j$ is the location of an error:

<p align="center">
<img
    alt="E = \left\{j \mid \Lambda(X_j^{-1}) = 0\right\}"
    src="https://latex.codecogs.com/svg.image?E%20%3d%20%5cleft%5c%7bj%20%5cmid%20%5cLambda%28X_j%5e%7b%2d%31%7d%29%20%3d%20%30%5cright%5c%7d"
>
</p>

Wikipedia and other resources often mention an optimization called
[Chien's search][w-chien] being applied here, but from reading up on the
algorithm it seems to only be useful for hardware implementations.  In
software Chien's search doesn't actually improve our runtime over brute
force with Horner's method and log tables ( $O(ne)$ vs $O(ne)$ ).

### Finding the error magnitudes

Once we've found the error-locations $X_j$, the next step is to find the
error-magnitudes $Y_j$.

This step is relatively straightforward... sort of...

Recall the definition of our syndromes $S_i$:

<p align="center">
<img
    alt="S_i = \sum_{j \in E} Y_j X_j^i"
    src="https://latex.codecogs.com/svg.image?S_i%20%3d%20%5csum_%7bj%20%5cin%20E%7d%20Y_j%20X_j%5ei"
>
</p>

With $e$ syndromes, this can be rewritten as a system with $e$ equations
and $e$ unknowns, which we can, in theory, solve for:

<p align="center">
<img
    alt="\begin{bmatrix} S_0 \\ S_1 \\ \vdots \\ S_{e-1} \end{bmatrix} = \begin{bmatrix} 1 & 1 & \dots & 1 \\ X_{j_0} & X_{j_1} & \dots & X_{j_{e-1}} \\ \vdots & \vdots & \ddots & \vdots \\ X_{j_0}^{e-1} & X_{j_1}^{e-1} & \dots & X_{j_{e-1}}^{e-1} \end{bmatrix} \begin{bmatrix} Y_{j_0} \\ Y_{j_1} \\ \vdots \\ Y_{j_{e-1}} \end{bmatrix}"
    src="https://latex.codecogs.com/svg.image?%5cbegin%7bbmatrix%7d%20S_%30%20%5c%5c%20S_%31%20%5c%5c%20%5cvdots%20%5c%5c%20S_%7be%2d%31%7d%20%5cend%7bbmatrix%7d%20%3d%20%5cbegin%7bbmatrix%7d%20%31%20%26%20%31%20%26%20%5cdots%20%26%20%31%20%5c%5c%20X_%7bj_%30%7d%20%26%20X_%7bj_%31%7d%20%26%20%5cdots%20%26%20X_%7bj_%7be%2d%31%7d%7d%20%5c%5c%20%5cvdots%20%26%20%5cvdots%20%26%20%5cddots%20%26%20%5cvdots%20%5c%5c%20X_%7bj_%30%7d%5e%7be%2d%31%7d%20%26%20X_%7bj_%31%7d%5e%7be%2d%31%7d%20%26%20%5cdots%20%26%20X_%7bj_%7be%2d%31%7d%7d%5e%7be%2d%31%7d%20%5cend%7bbmatrix%7d%20%5cbegin%7bbmatrix%7d%20Y_%7bj_%30%7d%20%5c%5c%20Y_%7bj_%31%7d%20%5c%5c%20%5cvdots%20%5c%5c%20Y_%7bj_%7be%2d%31%7d%7d%20%5cend%7bbmatrix%7d"
>
</p>

#### Forney's algorithm

But again, solving this system of equations is easier said than done.

Enter [Forney's algorithm][w-forney].

Assuming we know an error-locator $X_j$, the following formula will spit
out an error-magnitude $Y_j$:

<p align="center">
<img
    alt="Y_j = X_j \frac{\Omega(X_j^{-1})}{\Lambda'(X_j^{-1})}"
    src="https://latex.codecogs.com/svg.image?Y_j%20%3d%20X_j%20%5cfrac%7b%5cOmega%28X_j%5e%7b%2d%31%7d%29%7d%7b%5cLambda%27%28X_j%5e%7b%2d%31%7d%29%7d"
>
</p>

Where $\Omega(x)$, called the "error-evaluator polynomial", is defined
like so:

<p align="center">
<img
    alt="\Omega(x) = S(x) \Lambda(x) \bmod x^n"
    src="https://latex.codecogs.com/svg.image?%5cOmega%28x%29%20%3d%20S%28x%29%20%5cLambda%28x%29%20%5cbmod%20x%5en"
>
</p>

$S(x)$, the "syndrome polynomial", is defined like so (we just pretend
our syndromes are a polynomial now):

<p align="center">
<img
    alt="S(x) = \sum_{i=0}^{n-1} S_i x^i = S_0 + S_1 x + \cdots + S_{n-1} x^{n-1}"
    src="https://latex.codecogs.com/svg.image?S%28x%29%20%3d%20%5csum_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20S_i%20x%5ei%20%3d%20S_%30%20%2b%20S_%31%20x%20%2b%20%5ccdots%20%2b%20S_%7bn%2d%31%7d%20x%5e%7bn%2d%31%7d"
>
</p>

And $\Lambda'(x)$, the [formal derivative][w-formal-derivative] of the
error-locator, can be calculated like so:

<p align="center">
<img
    alt="\Lambda'(x) = \sum_{k=1}^e k \cdot \Lambda_k x^{k-1} = \Lambda_1 + 2 \cdot \Lambda_2 x + \cdots + e \cdot \Lambda_e x^{e-1}"
    src="https://latex.codecogs.com/svg.image?%5cLambda%27%28x%29%20%3d%20%5csum_%7bk%3d%31%7d%5ee%20k%20%5ccdot%20%5cLambda_k%20x%5e%7bk%2d%31%7d%20%3d%20%5cLambda_%31%20%2b%20%32%20%5ccdot%20%5cLambda_%32%20x%20%2b%20%5ccdots%20%2b%20e%20%5ccdot%20%5cLambda_e%20x%5e%7be%2d%31%7d"
>
</p>

Though note $k$ is not a field element, so multiplication by $k$
represents normal repeated addition. And since addition is xor in our
field, this really just cancels out every other term.

The end result is a simple formula for our error-magnitudes $Y_j$.

#### WTF

Haha, I know right? Where did this equation come from? Why does it work?
How did Forney even come up with this?

I don't know the answer to most of these questions, there's very little
information online about where/how/what this formula comes from.

But at the very least we can prove that it _does_ work.

#### The error-evaluator polynomial

Let's start with the syndrome polynomial $S(x)$:

<p align="center">
<img
    alt="S(x) = \sum_{i=0}^{n-1} S_i x^i = S_0 + S_1 x + \cdots + S_{n-1} x^{n-1}"
    src="https://latex.codecogs.com/svg.image?S%28x%29%20%3d%20%5csum_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20S_i%20x%5ei%20%3d%20S_%30%20%2b%20S_%31%20x%20%2b%20%5ccdots%20%2b%20S_%7bn%2d%31%7d%20x%5e%7bn%2d%31%7d"
>
</p>

Substituting in the definition of our syndromes
$S_i = \sum_{k \in E} Y_k X_k^i x^i$:

<p align="center">
<img
    alt="\begin{aligned} S(x) &= \sum_{i=0}^{n-1} \sum_{k \in E} Y_k X_k^i x^i \\ &= \sum_{k \in E} \left(Y_k \sum_{i=0}^{n-1} X_k^i x^i\right) \end{aligned}"
    src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20S%28x%29%20%26%3d%20%5csum_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20%5csum_%7bk%20%5cin%20E%7d%20Y_k%20X_k%5ei%20x%5ei%20%5c%5c%20%26%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5csum_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20X_k%5ei%20x%5ei%5cright%29%20%5cend%7baligned%7d"
>
</p>

The sum on the right turns out to be a [geometric series][w-geometric-series]:

<p align="center">
<img
    alt="S(x) = \sum_{k \in E} Y_k \frac{1 - X_k^n x^n}{1 - X_k x}"
    src="https://latex.codecogs.com/svg.image?S%28x%29%20%3d%20%5csum_%7bk%20%5cin%20E%7d%20Y_k%20%5cfrac%7b%31%20%2d%20X_k%5en%20x%5en%7d%7b%31%20%2d%20X_k%20x%7d"
>
</p>

If we then multiply with our error-locator polynomial
$\Lambda(x) = \prod_{l \in E} \left(1 - X_l x\right)$:

<p align="center">
<img
    alt="\begin{aligned} S(x)\Lambda(x) &= \sum_{k \in E} \left(Y_k \frac{1 - X_k^n x^n}{1 - X_k x}\right) \cdot \prod_{l \in E} \left(1 - X_l x\right) \\ &= \sum_{k \in E} \left(Y_k \left(1 - X_k^n x^n\right) \prod_{l \ne k} \left(1 - X_l x\right)\right) \end{aligned}"
    src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20S%28x%29%5cLambda%28x%29%20%26%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5cfrac%7b%31%20%2d%20X_k%5en%20x%5en%7d%7b%31%20%2d%20X_k%20x%7d%5cright%29%20%5ccdot%20%5cprod_%7bl%20%5cin%20E%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%20%5c%5c%20%26%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5cleft%28%31%20%2d%20X_k%5en%20x%5en%5cright%29%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29%20%5cend%7baligned%7d"
>
</p>

We see exactly one term in each summand cancel out, the term where
$l = k$.

At this point, if we plug in $X_j^{-1}$, $S(X_j^{-1})\Lambda(X_j^{-1})$
still evaluates to zero thanks to the error-locator polynomial
$\Lambda(x)$.

But if we expand the multiplication, something interesting happens:

<p align="center">
<img
    alt="S(x)\Lambda(x) = \sum_{k \in E} \left(Y_k \prod_{l \ne k} \left(1 - X_l x\right)\right) - \sum_{k \in E} \left(Y_k X_k^n x^n \prod_{l \ne k} \left(1 - X_l x\right)\right)"
    src="https://latex.codecogs.com/svg.image?S%28x%29%5cLambda%28x%29%20%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29%20%2d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20X_k%5en%20x%5en%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29"
>
</p>

On the left side of the subtraction, all terms are $\le$ degree
$x^{e-1}$. On the right side of the subtraction, all terms are $\ge$
degree $x^n$.

Imagine how these both contribute to the expanded form of the equation:

<p align="center">
<img
    alt="S(x)\Lambda(x) = \overbrace{\Omega_0 + \Omega_1 x + \cdots + \Omega_{e-1} x^{e-1}}^{\sum_{k \in E} \left(Y_k \prod_{l \ne k} \left(1 - X_l x\right)\right)} + \overbrace{\Omega_n x^n + \Omega_{n+1} x^{n+1} + \cdots + \Omega_{n+e-1} x^{n+e-1}}^{\sum_{k \in E} \left(Y_k X_k^n x^n \prod_{l \ne k} \left(1 - X_l x\right)\right) }"
    src="https://latex.codecogs.com/svg.image?S%28x%29%5cLambda%28x%29%20%3d%20%5coverbrace%7b%5cOmega_%30%20%2b%20%5cOmega_%31%20x%20%2b%20%5ccdots%20%2b%20%5cOmega_%7be%2d%31%7d%20x%5e%7be%2d%31%7d%7d%5e%7b%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29%7d%20%2b%20%5coverbrace%7b%5cOmega_n%20x%5en%20%2b%20%5cOmega_%7bn%2b%31%7d%20x%5e%7bn%2b%31%7d%20%2b%20%5ccdots%20%2b%20%5cOmega_%7bn%2be%2d%31%7d%20x%5e%7bn%2be%2d%31%7d%7d%5e%7b%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20X_k%5en%20x%5en%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29%20%7d"
>
</p>

If we truncate this polynomial, $\bmod x^n$ in math land, we can
effectively delete part of this equation:

<p align="center">
<img
    alt="S(x)\Lambda(x) \bmod x^n = \overbrace{\Omega_0 + \Omega_1 x + \dots + \Omega_{e-1} x^{e-1}}^{\sum_{k \in E} \left(Y_k \prod_{l \ne k} \left(1 - X_l x\right)\right)}"
    src="https://latex.codecogs.com/svg.image?S%28x%29%5cLambda%28x%29%20%5cbmod%20x%5en%20%3d%20%5coverbrace%7b%5cOmega_%30%20%2b%20%5cOmega_%31%20x%20%2b%20%5cdots%20%2b%20%5cOmega_%7be%2d%31%7d%20x%5e%7be%2d%31%7d%7d%5e%7b%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29%7d"
>
</p>

Giving us an equation for the error-evaluator polynomial, $\Omega(x)$:

<p align="center">
<img
    alt="\Omega(x) = S(x)\Lambda(x) \bmod x^n = \sum_{k \in E} \left(Y_k \prod_{l \ne k} \left(1 - X_l x\right)\right)"
    src="https://latex.codecogs.com/svg.image?%5cOmega%28x%29%20%3d%20S%28x%29%5cLambda%28x%29%20%5cbmod%20x%5en%20%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29"
>
</p>

Note that $l \ne k$ condition.

The error-evaluator polynomial $\Omega(x)$ still contains a big chunk
of our error-locator polynomial $\Lambda(x)$, so if we plug in an
error-location $X_j^{-1}$, _most_ of the terms evaluate to zero.
Except one! The one where $j = k$:

<p align="center">
<img
    alt="\begin{aligned} \Omega(X_j^{-1}) &= \sum_{k \in E} \left(Y_k \prod_{l \ne k} \left(1 - X_l X_j^{-1}\right)\right) \\ &= Y_j \prod_{l \ne j} \left(1 - X_l X_j^{-1}\right) \end{aligned}"
    src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20%5cOmega%28X_j%5e%7b%2d%31%7d%29%20%26%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28Y_k%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%5cright%29%20%5c%5c%20%26%3d%20Y_j%20%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%20%5cend%7baligned%7d"
>
</p>

And right there is our error-magnitude $Y_j$! Sure we end up with a
bunch of extra gobbledygook, but $Y_j$ _is_ there.

The good news is that gobbledygook depends only on our error-locations
$X_j$, which we _do_ know and can in theory remove with more math.

#### The formal derivative of the error-locator polynomial

But Forney has one last trick up his sleeve: The
[formal derivative][w-formal-derivative] of the error-locator polynomial,
$\Lambda'(x)$.

What the heck is a formal derivative?

Well we can't use normal derivatives in a finite-field like GF(256)
because they depend on the notion of a limit which depends on the field
being, well, not finite.

But derivatives are so useful mathematicians use them anyways.

Applying a formal derivative looks a lot like a normal derivative in
normal math:

<p align="center">
<img
    alt="f(x) = \sum_{i=0} f_i x^i = f_0 + f_1 x + \cdots + f_i x^i"
    src="https://latex.codecogs.com/svg.image?f%28x%29%20%3d%20%5csum_%7bi%3d%30%7d%20f_i%20x%5ei%20%3d%20f_%30%20%2b%20f_%31%20x%20%2b%20%5ccdots%20%2b%20f_i%20x%5ei"
>
</p>

<p align="center">
<img
    alt="f'(x) = \sum_{i=1} i \cdot f_i x^{i-1} = f_1 + 2 \cdot f_2 x + \dots + i \cdot f_i x^{i-1}"
    src="https://latex.codecogs.com/svg.image?f%27%28x%29%20%3d%20%5csum_%7bi%3d%31%7d%20i%20%5ccdot%20f_i%20x%5e%7bi%2d%31%7d%20%3d%20f_%31%20%2b%20%32%20%5ccdot%20f_%32%20x%20%2b%20%5cdots%20%2b%20i%20%5ccdot%20f_i%20x%5e%7bi%2d%31%7d"
>
</p>

Except $i$ here is not a finite-field element, so instead of doing
finite-field multiplication, we do normal repeated addition. And since
addition is xor in our field, this just has the effect of canceling out
every other term.

Quite a few properties of derivatives still hold in finite-fields. Of
particular interest to us is the [product rule][w-product-rule]:

<p align="center">
<img
    alt="\left(f(x) g(x)\right)' = f'(x) g(x) + f(x) g'(x)"
    src="https://latex.codecogs.com/svg.image?%5cleft%28f%28x%29%20g%28x%29%5cright%29%27%20%3d%20f%27%28x%29%20g%28x%29%20%2b%20f%28x%29%20g%27%28x%29"
>
</p>

<p align="center">
<img
    alt="\left(\prod_{i=0}^{n-1} f_i(x)\right)' = \sum_{i=0}^{n-1} \left(f_i'(x) \prod_{j \ne i} f_j(x)\right)"
    src="https://latex.codecogs.com/svg.image?%5cleft%28%5cprod_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20f_i%28x%29%5cright%29%27%20%3d%20%5csum_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20%5cleft%28f_i%27%28x%29%20%5cprod_%7bj%20%5cne%20i%7d%20f_j%28x%29%5cright%29"
>
</p>

Applying this to our error-locator polynomial $\Lambda(x)$:

<p align="center">
<img
    alt="\Lambda(x) = 1 + \sum_{k=1}^e \Lambda_k x^k = 1 + \Lambda_1 x + \Lambda_2 x^2 + \cdots + \Lambda_e x^e"
    src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%31%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20x%5ek%20%3d%20%31%20%2b%20%5cLambda_%31%20x%20%2b%20%5cLambda_%32%20x%5e%32%20%2b%20%5ccdots%20%2b%20%5cLambda_e%20x%5ee"
>
</p>

<p align="center">
<img
    alt="\Lambda'(x) = \sum_{k=1}^e k \cdot \Lambda_k x^{k-1} = \Lambda_1 + 2 \cdot \Lambda_2 x + \cdots + e \cdot \Lambda_e x^{e-1}"
    src="https://latex.codecogs.com/svg.image?%5cLambda%27%28x%29%20%3d%20%5csum_%7bk%3d%31%7d%5ee%20k%20%5ccdot%20%5cLambda_k%20x%5e%7bk%2d%31%7d%20%3d%20%5cLambda_%31%20%2b%20%32%20%5ccdot%20%5cLambda_%32%20x%20%2b%20%5ccdots%20%2b%20e%20%5ccdot%20%5cLambda_e%20x%5e%7be%2d%31%7d"
>
</p>

Recall the other definition of our error-locator polynomial $\Lambda(x)$:

<p align="center">
<img
    alt="\Lambda(x) = \prod_{k \in E} \left(1 - X_k x\right)"
    src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%5cprod_%7bk%20%5cin%20E%7d%20%5cleft%28%31%20%2d%20X_k%20x%5cright%29"
>
</p>

<p align="center">
<img
    alt="\Lambda'(x) = \left(\prod_{k \in E} \left(1 - X_k x\right)\right)'"
    src="https://latex.codecogs.com/svg.image?%5cLambda%27%28x%29%20%3d%20%5cleft%28%5cprod_%7bk%20%5cin%20E%7d%20%5cleft%28%31%20%2d%20X_k%20x%5cright%29%5cright%29%27"
>
</p>

Applying the product rule:

<p align="center">
<img
    alt="\Lambda'(x) = \sum_{k \in E} \left(X_k \prod_{l \ne k} \left(1 - X_l x\right)\right)"
    src="https://latex.codecogs.com/svg.image?%5cLambda%27%28x%29%20%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28X_k%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20x%5cright%29%5cright%29"
>
</p>

Starting to look familiar?

Just like the error-evaluator polynomial $\Omega(x)$, plugging in an
error-location $X_j^{-1}$ causes _most_ of the terms to evaluate to
zero, except the one where $j = k$, revealing $X_j$ times our
gobbledygook!

<p align="center">
<img
    alt="\begin{aligned} \Lambda'(X_j^{-1}) &= \sum_{k \in E} \left(X_k \prod_{l \ne k} \left(1 - X_l X_j^{-1}\right)\right) \\ &= X_j \prod_{l \ne j} \left(1 - X_l X_j^{-1}\right) \end{aligned}"
    src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20%5cLambda%27%28X_j%5e%7b%2d%31%7d%29%20%26%3d%20%5csum_%7bk%20%5cin%20E%7d%20%5cleft%28X_k%20%5cprod_%7bl%20%5cne%20k%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%5cright%29%20%5c%5c%20%26%3d%20X_j%20%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%20%5cend%7baligned%7d"
>
</p>

#### Evaluating the errors

So for a given error-location $X_j$, the error-evaluator polynomial
$\Omega(X_j^{-1})$ gives us the error-magnitude times some gobbledygook:

<p align="center">
<img
    alt="\Omega(X_j^{-1}) = Y_j \overbrace{\prod_{l \ne j} \left(1 - X_l X_j^{-1}\right)}^{\text{gobbledygook}}"
    src="https://latex.codecogs.com/svg.image?%5cOmega%28X_j%5e%7b%2d%31%7d%29%20%3d%20Y_j%20%5coverbrace%7b%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%7d%5e%7b%5ctext%7bgobbledygook%7d%7d"
>
</p>

And the formal derivative of the error-locator polynomial $\Lambda'(X_j^{-1})$
gives us the error-location times the same gobbledygook:

<p align="center">
<img
    alt="\Lambda'(X_j^{-1}) = X_j \overbrace{\prod_{l \ne j} \left(1 - X_l X_j^{-1}\right)}^{\text{gobbledygook}}"
    src="https://latex.codecogs.com/svg.image?%5cLambda%27%28X_j%5e%7b%2d%31%7d%29%20%3d%20X_j%20%5coverbrace%7b%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%7d%5e%7b%5ctext%7bgobbledygook%7d%7d"
>
</p>

If we divide $\Omega(X_j^{-1})$ by $\Lambda'(X_j^{-1})$, all that
gobbledygook cancels out, leaving us with a simply equation containing
only $Y_j$ and $X_j$:

<p align="center">
<img
    alt="\frac{\Omega(X_j^{-1})}{\Lambda'(X_j^{-1})} = \frac{Y_j \prod_{l \ne j} \left(1 - X_l X_j^{-1}\right)}{X_j \prod_{l \ne j} \left(1 - X_l X_j^{-1}\right)} = \frac{Y_j}{X_j}"
    src="https://latex.codecogs.com/svg.image?%5cfrac%7b%5cOmega%28X_j%5e%7b%2d%31%7d%29%7d%7b%5cLambda%27%28X_j%5e%7b%2d%31%7d%29%7d%20%3d%20%5cfrac%7bY_j%20%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%7d%7bX_j%20%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%7d%20%3d%20%5cfrac%7bY_j%7d%7bX_j%7d"
>
</p>

All that's left is to cancel out the $X_j$ term to get our
error-magnitude $Y_j$:

<p align="center">
<img
    alt="X_j \frac{\Omega(X_j^{-1})}{\Lambda'(X_j^{-1})} = X_j \frac{Y_j \prod_{l \ne j} \left(1 - X_l X_j^{-1}\right)}{X_j \prod_{l \ne j} \left(1 - X_l X_j^{-1}\right)} = Y_j"
    src="https://latex.codecogs.com/svg.image?X_j%20%5cfrac%7b%5cOmega%28X_j%5e%7b%2d%31%7d%29%7d%7b%5cLambda%27%28X_j%5e%7b%2d%31%7d%29%7d%20%3d%20X_j%20%5cfrac%7bY_j%20%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%7d%7bX_j%20%5cprod_%7bl%20%5cne%20j%7d%20%5cleft%28%31%20%2d%20X_l%20X_j%5e%7b%2d%31%7d%5cright%29%7d%20%3d%20Y_j"
>
</p>

### Putting it all together

Once we've figured out the error-locator polynomial $\Lambda(x)$, the
error-evaluator polynomial $\Omega(x)$, and the derivative of the
error-locator polynomial $\Lambda'(x)$, we get to the fun part, fixing
the errors!

For each location $j$ in the malformed codeword $C'(x)$, calculate the
error-location $X_j = g^j$ and plug its inverse $X_j^{-1}$ into the
error-locator $\Lambda(x)$. If $\Lambda(X_j^{-1}) = 0$ we've found the
location of an error!

<p align="center">
<img
    alt="E = \left\{j \mid \Lambda(X_j^{-1}) = 0\right\}"
    src="https://latex.codecogs.com/svg.image?E%20%3d%20%5cleft%5c%7bj%20%5cmid%20%5cLambda%28X_j%5e%7b%2d%31%7d%29%20%3d%20%30%5cright%5c%7d"
>
</p>

To fix the error, plug the error-location $X_j$ and its inverse
$X_j^{-1}$ into Forney's algorithm to find the error-magnitude
$Y_j$. Xor $Y_j$ into the codeword to fix this error!

<p align="center">
<img
    alt="Y_j = X_j \frac{\Omega(X_j^{-1})}{\Lambda'(X_j^{-1})}"
    src="https://latex.codecogs.com/svg.image?Y_j%20%3d%20X_j%20%5cfrac%7b%5cOmega%28X_j%5e%7b%2d%31%7d%29%7d%7b%5cLambda%27%28X_j%5e%7b%2d%31%7d%29%7d"
>
</p>

Repeat for all errors in the malformed codeword $C'(x)$, and with any
luck we'll find the original codeword $C(x)$!

<p align="center">
<img
    alt="C(x) = C'(x) - \sum_{j \in E} Y_j x^j"
    src="https://latex.codecogs.com/svg.image?C%28x%29%20%3d%20C%27%28x%29%20%2d%20%5csum_%7bj%20%5cin%20E%7d%20Y_j%20x%5ej"
>
</p>

Unfortunately we're not _quite_ done yet. All of this math assumed we had
$e \le \frac{n}{2}$ errors. If we had more errors, we might have just
made things worse.

It's worth recalculating the syndromes after fixing errors to see if we
did ended up with a valid codeword:

<p align="center">
<img
    alt="S_i = C(g^i) = 0"
    src="https://latex.codecogs.com/svg.image?S_i%20%3d%20C%28g%5ei%29%20%3d%20%30"
>
</p>

If the syndromes are all zero, chances are high we successfully repaired
our codeword.

Unless of course we had enough errors to end up overcorrecting to a
different codeword, but there's not much we can do in that case. No
error-correction is perfect.

This is all implemented in `ramrsbd_read`, if you're curious what it
looks like in code.

## Tricks

Heavy math aside, there are a couple minor implementation tricks worth
noting:

1. Truncate the generator polynomial to `ecc_size`.

   Calculating the generator polynomial for $n$ bytes of ECC gives us a
   polynomial with $n+1$ terms. This is a bit annoying since `ecc_size`
   is often a power-of-two:

   <p align="center">
   <img
       alt="P(x) = \prod_{i=0}^{n-1} \left(x - g^i\right) = \left(x-1\right)\left(x-g\right)\cdots\left(x-g^{n-1}\right)"
       src="https://latex.codecogs.com/svg.image?P%28x%29%20%3d%20%5cprod_%7bi%3d%30%7d%5e%7bn%2d%31%7d%20%5cleft%28x%20%2d%20g%5ei%5cright%29%20%3d%20%5cleft%28x%2d%31%5cright%29%5cleft%28x%2dg%5cright%29%5ccdots%5cleft%28x%2dg%5e%7bn%2d%31%7d%5cright%29"
   >
   </p>

   Fortunately, because of math reasons, the first term will always be 1.
   So, just like with our CRC polynomials, we can leave off the leading 1
   and make it implicit.

   Division with an implicit 1 is implemented in `ramrsbd_gf_p_divmod1`,
   which has the extra benefit of being able to skip the normalization
   step during [synthetic division][w-synthetic-division], so that's
   nice.

2. Store the generator polynomial in ROM.

   We don't really need to recompute the generator polynomial every time
   we initialize the block device, it's just convenient API-wise when we
   don't know `ecc_size`.

   If you only need to support a fixed set of block device geometries,
   precomputing and storing the generator polynomial in ROM will save a
   couple (`ecc_size`) bytes of RAM.

   [rs-poly.py][rs-poly.py] can help generate the generator polynomial:

   ``` bash
   $ ./rs-poly.py 8
   // generator polynomial for ecc_size=8
   //
   // P(x) = prod_i^n-1 (x - g^i)
   //
   static const uint8_t RAMRSBD_P[8] = {
       0xff, 0x0b, 0x51, 0x36, 0xef, 0xad, 0xc8, 0x18,
   };
   ```

   Which can then be provided to ramrsbd's `p` config option to avoid
   allocating the `p_buffer` in RAM.

   Unfortunately we still pay the code cost for generating the generator
   polynomial, which is difficult to avoid with the current API.

3. Minimizing the number of polynomial buffers.

   We have quite a few polynomials flying around:

   - $C(x)$ - Codeword buffer - `code_size` bytes
   - $P(x)$ Generator polynomial - `ecc_size` bytes (truncated)
   - $S_i$ - Syndrome buffer - `ecc_size` bytes
   - $\Lambda(x)$ - Error-locator polynomial - `ecc_size/2+1` (`<=ecc_size`) bytes
   - $C(i)$ - Connection polynomial - `ecc_size/2+1` (`<=ecc_size`) bytes
   - $\Omega(x)$ - Error-evaluator polynomial - `ecc_size/2` (`<=ecc_size`) bytes
   - $\Lambda'(x)$ - Derivative of the error-locator polynomial - `ecc_size/2` (`<=ecc_size`) bytes

   These get a bit annoying in a malloc-less system.

   Fortunately, there are a couple places we can reuse buffers:

   1. The connection polynomial $C(i)$ is only needed for
      Berlekamp-Massey and we can throw it away as soon as the
      error-locator $\Lambda(x)$ is found.

   2. We only need the syndrome buffer $S_i$ to find $\Lambda(x)$,
      $\Omega(x)$, and $\Lambda'(x)$. After that we have everything we
      need to start fixing errors.

   By sharing these buffers with polynomials computed later, such as the
   error-evaluator $\Omega(x)$ and derivative of the error-locator
   $\Lambda'(x)$, we can reduce the total number of buffers needed down
   to 1 `code_size` buffer and 4 `ecc_size` buffers (3 if the generator
   polynomial is stored in ROM).

4. Fused derivative evaluation.

   The formal derivative is a funny operation:

   <p align="center">
   <img
       alt="\Lambda(x) = 1 + \sum_{k=1}^e \Lambda_k x^k = 1 + \Lambda_1 x + \Lambda_2 x^2 + \cdots + \Lambda_e x^e"
       src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%31%20%2b%20%5csum_%7bk%3d%31%7d%5ee%20%5cLambda_k%20x%5ek%20%3d%20%31%20%2b%20%5cLambda_%31%20x%20%2b%20%5cLambda_%32%20x%5e%32%20%2b%20%5ccdots%20%2b%20%5cLambda_e%20x%5ee"
   >
   </p>
   
   <p align="center">
   <img
       alt="\Lambda'(x) = \sum_{k=1}^e k \cdot \Lambda_k x^{k-1} = \Lambda_1 + 2 \cdot \Lambda_2 x + \cdots + e \cdot \Lambda_e x^{e-1}"
       src="https://latex.codecogs.com/svg.image?%5cLambda%27%28x%29%20%3d%20%5csum_%7bk%3d%31%7d%5ee%20k%20%5ccdot%20%5cLambda_k%20x%5e%7bk%2d%31%7d%20%3d%20%5cLambda_%31%20%2b%20%32%20%5ccdot%20%5cLambda_%32%20x%20%2b%20%5ccdots%20%2b%20e%20%5ccdot%20%5cLambda_e%20x%5e%7be%2d%31%7d"
   >
   </p>

   Unlike other steps, it's a simple transformation that only affects one
   term at a time. This makes it easy to apply lazily without needing to
   copy the original polynomial.

   We already need to keep the error-locator polynomial $\Lambda(x)$
   around to, you know, locate the errors. So if we merge the derivative
   and evaluation of the error-locator into a single operation, we don't
   actually need to store the derivative of the error-locator
   $\Lambda'(x)$ as a separate polynomial. In theory reducing the number
   of buffers needed during error-evaluation from 3 down to 2.

   This sort of fused derivative evaluation is implemented in
   `ramrsbd_gf_p_deval` via a modified [Horner's method][w-horner].

   Unfortunately, we still need at least 3 buffers for Berlekamp-Massey
   ( $S_i$, $\Lambda(i)$, and $C(i)$ ), so this doesn't actually save us
   anything.

   But it doesn't really cost us anything either, cleans up the code a
   little bit, and lets us avoid clobbering the syndrome buffer $S_i$
   which is useful for debugging.

## Caveats

And some caveats:

1. For any error-correcting code, attempting to **correct** errors
   reduces the code's ability to **detect** errors.

   In Reed-Solomon's case, for $n$ bytes of ECC, we can detect up to
   $n$ byte-errors, but only correct up to
   $\left\lfloor\frac{n}{2}\right\rfloor$ byte-errors. Attempting to
   correct more errors can cause us to end up with a valid, but wrong,
   codeword.

   In practice this isn't that big of a problem. Fewer byte-errors are
   more common, and correcting byte-errors is usually more useful. At
   $n+1$ byte-errors you're going to end up with undetectable errors
   anyways.

   Still, it's good to be aware of this tradeoff.

   ramrsbd's `error_correction` config option lets you control exactly
   how many byte-errors to attempt to repair in case better detection is
   more useful.

2. Limited to 255 byte codewords - the non-zero elements of GF(256).

   An important step in Reed-Solomon is mapping each possible error
   location to a non-zero element of our finite field $X_j=g^j$.
   Unfortunately our finite-field is, well, finite, so there's only so
   many non-zero elements we can use before error-locations start to
   alias.

   This gives us a maximum codeword size of 255 bytes in GF(256),
   including the bytes used for ECC. A bit annoying, but math is math.

   In theory you can increase the maximum codeword size by using a larger
   finite-field, but this gets a bit tricky because the log/pow table
   approach used in ramrsbd stops being practical. 512 bytes of tables
   for GF(256) is fine, but 128 KiBs of tables for GF(2^16)? Not so
   much...

   1. If you have [carryless-multiplication][w-clmul] hardware available,
      GF(2^n) multiplication can be implemented efficiently by combining
      multiplication and [Barret reduction][w-barret-reduction].

      Division can then be implemented on top of multiplication by
      leveraging the fact that $a^{2^n-2} = a^{-1}$ for any element $a$
      in GF(2^n). [Binary exponentiation][w-binary-exponentiation] can
      make this somewhat efficient.

   2. In the same way GF(256) is defined as an
      [extension field][w-extension-field] of GF(2), we can define
      GF(2^16) as an extension field of GF(256), where each element is a
      2 byte polynomial containing digits in GF(256).

      This can be convenient if you already need GF(256) tables for other
      parts of the codebase.

   Or, a simpler alternative, you can just pack multiple "physical"
   codewords into one "logical" codeword.

   You could even consider interleaving the physical codewords if you
   want to maintain the systematic encoding or are trying to protect
   against specific error patterns.

3. Support for known-location "erasures" left as an exercise for the
   reader.

   All of the above math assumes we don't know the location of errors,
   which is the most common case for block devices.

   But it turns out if we _do_ know the location of errors, via parity
   bits or some other side-channel, we can do quite a bit better. We
   usually call these known-location errors "erasures".

   With Reed-Solomon, each unknown-location error requires 2 bytes of ECC
   to find and repair, while known-location erasures require only 1 byte
   of ECC to repair. You can even mix and match $e$ errors and $f$
   erasures as long as you have $n$ bytes of ECC such that:

   <p align="center">
   <img
       alt="2e + f \le n"
       src="https://latex.codecogs.com/svg.image?%32e%20%2b%20f%20%5cle%20n"
   >
   </p>

   This isn't implemented in ramrsbd, but, _in theory_, the math isn't
   too difficult to extend.

   First note we can split $\Lambda(x)$ into a separate error-locator
   poylnomial $\Lambda_E(x)$ and erasure-locator polynomial
   $\Lambda_F(x)$:

   <p align="center">
   <img
       alt="\Lambda(x) = \Lambda_E(x) \Lambda_F(x)"
       src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%5cLambda_E%28x%29%20%5cLambda_F%28x%29"
   >
   </p>

   We know the location of the known-location erasures, so the
   erasure-locator $\Lambda_F(x)$ is trivial to calculate:

   <p align="center">
   <img
       alt="\Lambda_F(x) = \prod_{j \in F} \left(1 - X_j x\right)"
       src="https://latex.codecogs.com/svg.image?%5cLambda_F%28x%29%20%3d%20%5cprod_%7bj%20%5cin%20F%7d%20%5cleft%28%31%20%2d%20X_j%20x%5cright%29"
   >
   </p>

   Before we can find the error-locator polynomial $\Lambda_E(x)$, we
   need to modify our syndromes the hide the effects of the
   erasure-locator polynomial. These are often called the Forney
   syndromes $S_{Fi}$:

   <p align="center">
   <img
       alt="S_F(x) = S(x) \Lambda_F(x) \bmod x^n"
       src="https://latex.codecogs.com/svg.image?S_F%28x%29%20%3d%20S%28x%29%20%5cLambda_F%28x%29%20%5cbmod%20x%5en"
   >
   </p>

   Note that the Forney syndromes $S_{Fi}$ sill satisfy the equation for
   $\Omega(x)$:

   <p align="center">
   <img
       alt="\begin{aligned} \Omega(x) &= S(x)\Lambda(x) \bmod x^n \\ &= S(x)\Lambda_E(x)\Lambda_F(x) \bmod x^n \\ &= S_F(x)\Lambda_E(x) \bmod x^n \end{aligned}"
       src="https://latex.codecogs.com/svg.image?%5cbegin%7baligned%7d%20%5cOmega%28x%29%20%26%3d%20S%28x%29%5cLambda%28x%29%20%5cbmod%20x%5en%20%5c%5c%20%26%3d%20S%28x%29%5cLambda_E%28x%29%5cLambda_F%28x%29%20%5cbmod%20x%5en%20%5c%5c%20%26%3d%20S_F%28x%29%5cLambda_E%28x%29%20%5cbmod%20x%5en%20%5cend%7baligned%7d"
   >
   </p>

   We can then use Berlekamp-Massey with the Forney syndromes $S_{Fi}$ to
   find the error-locator polynomial $\Lambda_E(x)$.

   Combining the error-locator polynomial $\Lambda_E(x)$ and the
   erasure-locator polynomial $\Lambda_F(x)$ gives us the creatively
   named error-and-erasure-locator-polynomial $\Lambda(x)$, which
   contains everything we need to know to find the location of both
   errors and erasures:

   <p align="center">
   <img
       alt="\Lambda(x) = \Lambda_E(x) \Lambda_F(x)"
       src="https://latex.codecogs.com/svg.image?%5cLambda%28x%29%20%3d%20%5cLambda_E%28x%29%20%5cLambda_F%28x%29"
   >
   </p>

   At this point we can continue Reed-Solomon as normal, finding the
   error/easures locations where $\Lambda(X_j^{-1})=0$, and repairing
   them with Forney's algorithm,
   $Y_j = X_j \frac{\Omega(X_j^{-1})}{\Lambda'(X_j^{-1})}$:

   <p align="center">
   <img
       alt="C(x) = C'(x) - \sum_{j \in E \cup F} Y_j x^j"
       src="https://latex.codecogs.com/svg.image?C%28x%29%20%3d%20C%27%28x%29%20%2d%20%5csum_%7bj%20%5cin%20E%20%5ccup%20F%7d%20Y_j%20x%5ej"
   >
   </p>

## References

- [Massey, J. L. - Shift-register synthesis and BCH decoding][massey-srsbchd]
- [Gill, J. - EE387 Notes #7][gill-notes7]
- [Truong, T. K., Hsu, I. S., Eastman, W. L., Reed, I. S. - A Simplified Procedure for Correcting Both Errors and Erasures of a Reed-Solomon Code Using the Euclidean Algorithm][truong-spcbeerscuea]

- [Wikiversity - Reed Solomon for coders][wikiversity-rs]

- [Wikipedia - Reed Solomon][w-rs]
- [Wikipedia - Berlekamp-Massey algorithm][w-bm]
- [Wikipedia - Forney Algorithm][w-forney]

- [Wikipedia - BCH Code][w-bch]
- [Wikipedia - Finite field arithmetic][w-gf]
- [Wikipedia - GF(2)][w-gf2]
- [Wikipedia - Systematic Code][w-systematic-code]
- [Wikipedia - Primitive element (finite field)][w-generator]
- [Wikipedia - Linear-feedback shift register (LFSR)][w-lfsr]
- [Wikipedia - Recurrence Relation][w-recurrence-relation]
- [Wikipedia - Chien search][w-chien]
- [Wikipedia - Formal derivative][w-formal-derivative]
- [Wikipedia - Geometric series][w-geometric-series]
- [Wikipedia - Product rule][w-product-rule]
- [Wikipedia - Synthetic division][w-synthetic-division]
- [Wikipedia - Horner's method][w-horner]
- [Wikipedia - Carry-less product][w-clmul]
- [Wikipedia - Barret reduction][w-barret-reduction]
- [Wikipedia - Exponentiation by squaring][w-binary-exponentiation]
- [Wikipedia - Field extension][w-extension-field]

[massey-srsbchd]: http://crypto.stanford.edu/~mironov/cs359/massey.pdf
[gill-notes7]: https://web.archive.org/web/20140630172526/http://web.stanford.edu/class/ee387/handouts/notes7.pdf
[truong-spcbeerscuea]: https://ntrs.nasa.gov/api/citations/19880003316/downloads/19880003316.pdf

[wikiversity-rs]: https://en.wikiversity.org/wiki/Reed%E2%80%93Solomon_codes_for_coders

[w-rs]: https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction
[w-bm]: https://en.wikipedia.org/wiki/Berlekamp%E2%80%93Massey_algorithm
[w-forney]: https://en.wikipedia.org/wiki/Forney_algorithm

[w-bch]: https://en.wikipedia.org/wiki/BCH_code
[w-pgz]: https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction#Peterson%E2%80%93Gorenstein%E2%80%93Zierler_decoder
[w-euclidean]: https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction#Euclidean_decoder
[w-gf]: https://en.wikipedia.org/wiki/Finite_field_arithmetic
[w-gf2]: https://en.wikipedia.org/wiki/GF(2)
[w-gf256]: https://en.wikipedia.org/wiki/Finite_field_arithmetic#Effective_polynomial_representation
[w-systematic-code]: https://en.wikipedia.org/wiki/Systematic_code
[w-generator]: https://en.wikipedia.org/wiki/Primitive_element_(finite_field)
[w-lfsr]: https://en.wikipedia.org/wiki/Linear-feedback_shift_register
[w-recurrence-relation]: https://en.wikipedia.org/wiki/Recurrence_relation
[w-chien]: https://en.wikipedia.org/wiki/Chien_search
[w-formal-derivative]: https://en.wikipedia.org/wiki/Formal_derivative
[w-geometric-series]: https://en.wikipedia.org/wiki/Geometric_series
[w-product-rule]: https://en.wikipedia.org/wiki/Product_rule
[w-synthetic-division]: https://en.wikipedia.org/wiki/Synthetic_division
[w-horner]: https://en.wikipedia.org/wiki/Horner%27s_method
[w-clmul]: https://en.wikipedia.org/wiki/Carry-less_product
[w-barret-reduction]: https://en.wikipedia.org/wiki/Barrett_reduction
[w-binary-exponentiation]: https://en.wikipedia.org/wiki/Exponentiation_by_squaring
[w-extension-field]: https://en.wikipedia.org/wiki/Field_extension

[littlefs]: https://github.com/littlefs-project/littlefs
[ramcrc32bd]: https://github.com/geky/ramcrc32bd

[bm-lfsr-solver.py]: bm-lfsr-solver.py
[bm-lfsr256-solver.py]: bm-lfsr256-solver.py
[rs-poly.py]: rs-poly.py

