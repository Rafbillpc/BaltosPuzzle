Balto's puzzle
===

This repository contains my solution to Al Zimmermann's programming contest [Balto's Puzzle](http://azspcs.com/Contest/BaltosPuzzle).

The problem was a variant of the (N²-1)-puzzle:

- Instead of a square grid, the puzzle takes place on a hexagonal grid.
- Instead of transpositions of two adjacent tiles, the basic moves are rotations of three adjacent tiles. There are 12 such moves.
- Our of the 12 basic moves, either the clockwise or the counter-clockwise rotations are available at a given time.

Approach
---

This kind of puzzle is a permutation puzzle. 
Typically solving permutation puzzles involve two kinds of approaches:

- **Insertions**: given a partial solution, we can insert a short sequence of moves (e.g. a commutator) that only affects a small number of pieces.
By inserting such sequences in the middle of a partial solution, some moves can cancel out, and 
- **Beam search**, for a given heuristic estimation of the distance from a state from to the goal state.

Typically, insertions work well for the Rubik's cube (when the basic moves affect many pieces, but all pieces start close to their target positions),
and beam search works well for the (N²-1)-puzzle (when the basic moves affect very few pieces, but pieces can be far from their target positions).
But for both kinds of puzzles, I would expect the best solutions to use a combination of both.

I only used beam search, but my solution struggles to fix the last few pieces. Trying some insertions when almost all pieces are solved could be an improvement.

I used my beam search implementation ([Euler tour beam search](https://gitlab.com/rafaelbocquet-cpcontests/euler-tour-beam-search)), which avoids state copying. As a result, the time needed for a single beam search iteration for a given beam width does not depend on the problem size (but the number of iterations is the size of the solution, which grows cubically with the problem size).

The state I use for beam search consists of a both a source and a target puzzle board.
The puzzle is solved when the two boards coincide (including the direction of the last rotation).
Moves can be applied either to the source or the target board.
This increases the branching factor from 6 to 12.

The heuristic
---

My starting point was wata's heuristic for the (N²-1)-puzzle ([code](https://atcoder.jp/contests/ahc011/submissions/32269562)).
One of the main observations is that the manhattan distance $`(dx+dy)`$ should be replaced by $`(dx²+dy²)`$ in beam search:
the manhattan distance is an admissible heuristic (suitable for exact search using A*), 
but beam search benefits from a more precise heuristic, even if it is not admissible.

The heuristic has a first component
```math
H_1 = \sum_{(dx,dy)} F_1(dx,dy).
```
where $`(dx,dy)`$ ranges over distances between source and target positions for a given piece,
and $`F_1`$ is a function to be determined later.

The best closed expression I found for $`F_1`$ was
```math
F_1(dx,dy) = 3 * (dx*dx+dx*dy+dy*dy) + 7 * (dx+dy).
```

Using just distances is problematic at the end of the solving process: we end up with lots of pieces within distance 0-2 of their target positions, but the solved pieces are distributed somewhat uniformly through the board. 
It becomes harder to solve additional pieces without disturbing the pieces which were already solved.

Ideally the solved pieces should form a single connected component.
Furthermore, the edges of this connected component should be smooth.

To solve this, I added a penalty based on the neighborhood of an unsolved piece:

- An unsolved piece that is completely surrounded by solved pieces is very bad.
- An unsolved piece that is surrounded by 5 solved pieces is also quite bad.
- An unsolved piece that is surrounded by 4 solved pieces can be bad, but this depends on the exact configuration: 110110 is worse than 111100.

The neighborhood of a unsolved piece is represented as a bitstring of length 6.
The i-th bit indicates whether the neighboor in the i-th direction is solved or not.
These are quotiented by rotations and symmetry, e.g. 110100 is identified with 010110.

The second component of the heuristic is
```math
H_2 = \sum_{w} F_2(w),
```
where $`w`$ ranges over the neighborhood of unsolved pieces, and $`F_2(w)`$ is the weight assigned to 

My first guess was to define $`F_2(w)`$ as the Hamming weight of $`w`$.

The total heuristic is the sum of $`H_1`$ and $`H_2`$.

Training the heuristic
---

I was unsure of how to best define $`F_1`$ and $`F_2`$, so I decided to determine them using machine learning.

The total heuristic can be described as a sum of weights assigned to features, i.e. $`H = W^T X`$ for a weight vector $`W`$ and a feature vector $`X`$.
There are up to 209 features: 196 possible distances between source and target positions (for n=27), and 13 possible neighborhood.

I searched in the literature for ways to learn a heuristic, but couldn't find a suitable method.
Existing approaches (e.g. [DeepCubeA](https://github.com/forestagostinelli/DeepCubeA)) usually try to learn the distance to the target state. 
It seems better to learn how to compare states, because beam search only depends on such an ordering between states.
Other approaches would require an expensive search to generate a single training sample.

The breakthrough came from the idea that we could use beam search to generate many training samples efficiently.
A training sample is a pair $`(X_1, X_2)`$ 
where $`X_1`$, $`X_2`$ are feature vectors,
and we want $`X_1`$ to be ranked better than $`X_2`$.
Run beam search with a random initial state and a smallish beam width (between 500 and 4000).
At some point, beam search terminates with the target state, and there is a path from the initial state to the target state going through every level of the beam search.
At a given level, let $`X_1`$ be the feature vector state that is part of this path, and $`X_2`$ be the feature vector of any other state at that level.
Then I include $`(X_1, X_2)`$ as a training sample (with some probability, in order to avoid collecting samples that are too similar).

This procedure is repeated until enough training samples have been collected (between 10^6 and 10^7).

For training, gradient descent is used to minimize (as a function of $`W`$) the sum of 
```math
\mathsf{tanh}(W^T X_1 - W^T X_2)
```
over all training samples.

