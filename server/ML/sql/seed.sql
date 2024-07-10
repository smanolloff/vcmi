--
-- Query to seed `stats` table with data.
-- Given the number of heroes, it will insert a row with wins=0 and games=0
--  for each possible pair of hero IDs.
--
-- Parameters:
--  1: number of heroes on the map
--  2: the side from which stats are collected (0 for left, 1 for right side)
--
-- To seed for 8 heroes and side 0:
--  $ cat seed.sql |
--      sed -e "s/--.*//" -e "1,/\?/s/\?/8/" -e "1,/\?/s/\?/0/" |
--      sqlite3 stats.db
--
BEGIN;
WITH RECURSIVE
    hero_ids AS (
        SELECT 0 AS n
        UNION ALL
        SELECT n + 1
        FROM hero_ids
        WHERE n < (? - 1)
    ),
    data AS (
        SELECT lhero_ids.n AS lhero,
               rhero_ids.n AS rhero
        FROM hero_ids AS lhero_ids,
             hero_ids AS rhero_ids
        WHERE lhero != rhero
    )
INSERT INTO stats (lhero, rhero, wins, games)
SELECT lhero, rhero, 0, 0
FROM data;

INSERT INTO stats_md (side) VALUES (?);
COMMIT;
