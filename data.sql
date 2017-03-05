-- If there is already any data in these tables, empty it out.
-- always remember 
-- ssh dbsrv1.cdf.utoronto.ca (or psql csc343h-sunlin3 then ssh dbsrv1.cdf.utoronto.ca) then \i data.sql

TRUNCATE TABLE Result CASCADE;
TRUNCATE TABLE Grade CASCADE;
TRUNCATE TABLE RubricItem CASCADE;
TRUNCATE TABLE Grader CASCADE;
TRUNCATE TABLE Submissions CASCADE;
TRUNCATE TABLE Membership CASCADE;
TRUNCATE TABLE AssignmentGroup CASCADE;
TRUNCATE TABLE Required CASCADE;
TRUNCATE TABLE Assignment CASCADE;
TRUNCATE TABLE MarkusUser CASCADE;


-- Now insert data from scratch.

INSERT INTO MarkusUser VALUES ('i1', 'iln1', 'ifn1', 'instructor');
INSERT INTO MarkusUser VALUES ('s1', 'sln1', 'sfn1', 'student');
INSERT INTO MarkusUser VALUES ('s2', 'sln2', 'sfn2', 'student');
INSERT INTO MarkusUser VALUES ('t1', 'tln1', 'tfn1', 'TA');

INSERT INTO Assignment VALUES (1000, 'a1', '2017-02-08 20:00', 1, 2);

INSERT INTO Required VALUES (1000, 'A1.pdf');

INSERT INTO AssignmentGroup VALUES (2000, 1000, 'repo_url');

INSERT INTO Membership VALUES ('s1', 2000);
INSERT INTO Membership VALUES ('s2', 2000);

INSERT INTO Submissions VALUES (3000, 'A1.pdf', 's1', 2000, '2017-02-08 19:59');

INSERT INTO Grader VALUES (2000, 't1');

INSERT INTO RubricItem VALUES (4000, 1000, 'style', 4, 0.25);
INSERT INTO RubricItem VALUES (4001, 1000, 'tester', 12, 0.75);

INSERT INTO Grade VALUES (2000, 4000, 3);
INSERT INTO Grade VALUES (2000, 4001, 9);

INSERT INTO Result VALUES (2000, 12, true);
