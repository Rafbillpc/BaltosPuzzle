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

- Insertions: given a partial solution, we can insert a short sequence of moves (e.g. a commutator) that only affects a small number of pieces.
By inserting such sequences in the middle of a partial solution, some moves can cancel out, and 
- Beam search, for a given heuristic estimation of the distance from a state from to the goal state.

Typically, insertions work well for the Rubik's cube (when the basic moves affect many pieces, but all pieces start close to their target positions),
and beam search works well for the (N²-1)-puzzle (when the basic moves affect very few pieces, but pieces can be far from their target positions).
But for both kinds of puzzles, I would expect the best solutions to use a combination of both.

I only used beam search, but my solution struggles to fix the last few pieces. Trying some insertions when almost all pieces are solved could be an improvement.

I used my beam search implementation ([Euler tour beam search](https://gitlab.com/rafaelbocquet-cpcontests/euler-tour-beam-search)), which avoids state copying. As a result, the time needed for a single beam search iteration for a given beam width do not depend on the problem size (but the number of iterations is the size of the solution, which grows cubically with the problem size).

The state I use for beam search consists of a both a source and a target puzzle board.
The puzzle is solved when the two boards coincide (including the direction of the last rotation).
Moves can be applied either to the source or the target board.
This increases the branching factor from 6 to 12.

The heuristic
---

My starting point was wata's heuristic for the (N²-1)-puzzle ([code](https://atcoder.jp/contests/ahc011/submissions/32269562)).
One of the main observations is that the manhattan distance $`(dx+dy)`$ should be replaced by $`(dx²+dy²)`$ in beam search: the manhattan distance is an admissible heuristic (suitable for exact search using A*), but beam search benefits from a more precise heuristic, even if it is not admissible.

The heuristic has a first component
$$ H_1 = \sum_{(dx,dy)} F_1(dx,dy). $$
where $`(dx,dy)`$ ranges over distances between source and target positions for a given piece,
and $`F_1`$ is a function to be determined later.

The best closed expression I found for $`F_1`$ was
$$ F_1(dx,dy) = 3 * (dx*dx+dx*dy+dy*dy) + 7 * (dx+dy). $$

Using just distances is problematic at the end of the solving process: we end up with lots of pieces within distance 0-2 of their target positions, but the solved pieces are distributed somewhat uniformly through the board. 
It becomes harder to solve additional pieces without disturbing the pieces which were already solved.

To solve this, I added a penalty based on the neighborhood of an unsolved piece:

- An unsolved piece that is completely surrounded by solved pieces is very bad.
- An unsolved piece that is surrounded by 5 solved pieces is also quite bad.
- An unsolved piece that is surrounded by 4 solved pieces can be bad, but this depends on the exact configuration: 110110 is worse than 111100.

The neighborhood of a unsolved piece is represented as a bitstring of length 6.
The i-th bit indicates whether the neighboor in the i-th direction is solved or not.
These are quotiented by rotations and symmetry, e.g. 110100 is identified with 010110.

The second component of the heuristic is
$$ H_2 = \sum_{w} F_2(w), $$
where $`w`$ ranges over the neighborhood of unsolved pieces, and $`F_2(w)`$ is the weight assigned to 

For example, my first guess was to define $`F_2(w)`$ as the Hamming weight of $`w`$.

The total heuristic is the sum of $`H_1`$ and $`H_2`$.

Training the heuristic
---

TODO
