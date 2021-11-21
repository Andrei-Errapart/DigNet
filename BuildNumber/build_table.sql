-- Database creation for the BuildServer
-- --------------------------------------------------------------------
drop table if exists ProjectName;
create table ProjectName (
    -- Row ID
    id  	int       not null auto_increment,
    -- Name
    Name	char(255) not null,
    Primary     key(id)
); -- ProjectName

drop table if exists ProjectBuild;
create table ProjectBuild (
    -- Row ID
    id            int         not null auto_increment,
    -- ID of the project
    Project_ID    int         not null,
    -- Build date
    Build_Date    date        not null,
    -- Sequental build number
    Build_Number  int         not null,
    -- Version number of build
    Build_Version int         not null,
    -- Version of base at the time of building
    Build_Base_Version int    not null,
    Primary       key(id)
); -- ProjectBuild

