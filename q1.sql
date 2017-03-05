-- Distributions

SET SEARCH_PATH TO markus;
DROP TABLE IF EXISTS q1;

-- You must not change this table definition.
CREATE TABLE q1 (
	assignment_id integer,
	average_mark_percent real, 
	num_80_100 integer, 
	num_60_79 integer, 
	num_50_59 integer, 
	num_0_49 integer
);

-- You may find it convenient to do this for each of the views
-- that define your intermediate steps.  (But give them better names!)
DROP VIEW IF EXISTS 80_100_data CASCADE;

-- Define views for your intermediate steps here.
CREATE VIEW absolute_mark AS
	SELECT assignment_id, group_id, mark
	FROM AssignmentGroup NATURAL JOIN RESULT
	

-- Final answer.
INSERT INTO q1 (
	SELECT
		
	);

	
