--
-- Query to seed `stats` table with data.
-- Given the number of heroes, it will insert a row with wins=0 and games=0
--  for each possible pair of hero IDs.
--
-- Parameters:
--  1: number of heroes on the map
--
-- To seed for 8 heroes:
--  $ cat seed.sql | sed -e "s/--.*//" -e "1,/\?/s/\?/8/" | sqlite3 stats.db
--

WITH RECURSIVE
    hero_ids AS (
        SELECT 0 AS n
        UNION ALL
        SELECT n + 1
        FROM hero_ids
        WHERE n < (? - 1)
    ),
    sides AS (
        SELECT 0 AS n
        UNION ALL
        SELECT 1 AS n
    ),
    data AS (
        SELECT sides.n AS side,
               lhero_ids.n AS lhero,
               rhero_ids.n AS rhero
        FROM sides,
             hero_ids AS lhero_ids,
             hero_ids AS rhero_ids
        WHERE lhero != rhero
    )
INSERT INTO stats (side, lhero, rhero, wins, games)
SELECT side, lhero, rhero, 0, 0
FROM data;
