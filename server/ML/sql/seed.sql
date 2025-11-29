--
-- Query to seed `stats` table with data.
-- Given the number of heroes, it will insert a row with wins=0 and games=0
--  for each possible pair of hero IDs within the given number of pools.
--
-- Parameters:
--  1: number of pools
--  2: number of heroes in one pool
--  3: the side from which stats are collected (0 for left, 1 for right side)
--
-- To seed for 2 pools, 4 heroes per pool and side 0:
--  $ sed -e "s/--.*//" -e "1,/\?/s/\?/2/" -e "1,/\?/s/\?/4/" -e "1,/\?/s/\?/0/" seed.sql | sqlite3 stats.db
--
BEGIN;

INSERT INTO stats_md (n_pools, pool_size, side) VALUES (?, ?, ?);

WITH RECURSIVE
    pools AS (
        SELECT 0 AS id
        UNION ALL
        SELECT id + 1
        FROM pools, stats_md
        WHERE id + 1 < stats_md.n_pools
    ),
    heroes AS (
        SELECT 0 AS id
        UNION ALL
        SELECT id + 1
        FROM heroes, stats_md
        WHERE id + 1 < stats_md.pool_size
    ),
    data AS (
        SELECT
            pools.id as pool,
            (pools.id * stats_md.pool_size) + lheroes.id AS lhero,
            (pools.id * stats_md.pool_size) + rheroes.id AS rhero
        FROM heroes AS lheroes, heroes AS rheroes, pools, stats_md
        WHERE lheroes.id != rheroes.id
        ORDER BY pool, lhero, rhero
    )
INSERT INTO stats (pool, lhero, rhero, wins, games)
SELECT pool, lhero, rhero, 0, 0
FROM data;

COMMIT;
