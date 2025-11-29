--
-- Query to create the `stats` table.
--
-- To run the query:
--  $ sqlite3 stats.db < structure.sql
--
BEGIN;
CREATE TABLE stats (
  id INTEGER primary key,
  pool INTEGER NOT NULL,
  lhero INTEGER NOT NULL,
  rhero INTEGER NOT NULL,
  wins INTEGER NOT NULL,
  games INTEGER NOT NULL
);

CREATE UNIQUE INDEX stats_idx
ON stats(pool, lhero, rhero);

CREATE TABLE stats_md (
  side INTEGER,
  n_pools INTEGER,
  pool_size INTEGER,
  PRIMARY KEY (side, n_pools, pool_size)
);
COMMIT;
