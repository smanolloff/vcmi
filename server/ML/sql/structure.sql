--
-- Query to create the `stats` table.
--
-- To run the query:
--  $ sqlite3 stats.db < structure.sql
--

CREATE TABLE stats (
  id INTEGER primary key,
  side INTEGER NOT NULL,
  lhero INTEGER NOT NULL,
  rhero INTEGER NOT NULL,
  wins INTEGER NOT NULL,
  games INTEGER NOT NULL
);

CREATE UNIQUE INDEX stats_idx
ON stats(side, lhero, rhero);
