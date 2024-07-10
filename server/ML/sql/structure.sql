--
-- Query to create the `stats` table.
--
-- To run the query:
--  $ sqlite3 stats.db < structure.sql
--
BEGIN;
CREATE TABLE stats (
  id INTEGER primary key,
  lhero INTEGER NOT NULL,
  rhero INTEGER NOT NULL,
  wins INTEGER NOT NULL,
  games INTEGER NOT NULL
);

CREATE UNIQUE INDEX stats_idx
ON stats(lhero, rhero);

CREATE TABLE stats_md (side INTEGER primary key);
COMMIT;
