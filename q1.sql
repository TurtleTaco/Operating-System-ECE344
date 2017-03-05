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
DROP VIEW IF EXISTS assignment_group_mark CASCADE;

-- Define views for your intermediate steps here.
CREATE VIEW assignment_group_ABS_mark (assignment_id, group_id, mark) AS
	SELECT assignment_id, group_id, mark
	FROM AssignmentGroup NATURAL JOIN RESULT;

CREATE VIEW assignment_full_mark (assignment_id, full_mark) AS
	SELECT assignment_id, sum(out_of * weight)
	FROM RubricItem
	GROUP BY assignment_id;

CREATE VIEW assignment_group_real_mark (real_mark, group_id, assignment_id) AS
	SELECT cast(mark as float)/cast(full_mark as float), group_id, assignment_id
	FROM assignment_group_ABS_mark NATURAL JOIN assignment_full_mark;

-- Final answer.
INSERT INTO q1 
	-- put a final query here so that its results will go into the table.
	(
		SELECT count(case when real_mark >= 80 and real_mark <= 100 then 1 else 0 end) as num_80_100,
			   count(case when real_mark >= 60 and real_mark <= 79 then 1 else 0 end) as num_60_79,
			   count(case when real_mark >= 50 and real_mark <= 59 then 1 else 0 end) as num_50_59,
			   count(case when real_mark >= 0 and real_mark <= 49 then 1 else 0 end) as num_0_49
		FROM assignment_group_real_mark
		GROUP BY assignment_id
	)