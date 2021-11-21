-- Database creation for the GpsServer
-- --------------------------------------------------------------------
drop table if exists Ships;
create table Ships (
	-- Row ID
	id		int                 not null auto_increment,
	-- Is Ship public (yet)?
	IsPublic	char(1)			not null default 'N',
	-- Group ID
	GroupId     int             not null default 0,
	-- Sim number of Modembox
	SimNumber   char(255)       not null,
	-- type of ship
	ShipType    char(255)       not null,
	-- IMEI = Hardware serial no.
	IMEI		char(255)		not null,
	-- FIRMWARE name as given in the protocol.
	Firmware_Name	char(255)		not null,
	-- Firmware version.
	Firmware_Version	char(255)	not null,
	-- Firmware build date (if possible to decode).
	Firmware_Build_date	datetime	not null,
	-- Phone number.
	Phone_no	char(255)		not null,
	-- Name
	Name		char(255)		not null,
	-- Minimum Voltage under what warnings will be shown
	SupplyVoltageLimit	double	not null,
	-- Last contact time (data byte received or connection setup).
	Contact_Time	datetime,

	-- GPS1 last received time, null if not valid.
	GPS1_Time	datetime,
	GPS1_X		double not null default 0,
	GPS1_Y		double not null default 0,
	GPS2_Time	datetime,
	GPS2_X		double not null default 0,
	GPS2_Y		double not null default 0,

	-- Heading last calculated time, null if not valid.
	Heading_Time	datetime,
	Heading		double not null default 0,
	
	-- Gps positions relative to ship
	Gps1_dx		double not null default 0,
	Gps1_dy		double not null default 0,
	Gps2_dx		double not null default 0,
	Gps2_dy		double not null default 0,
	
	-- Shadow position coordinates
	Shadow_X	double not null default -1,
	Shadow_Y	double not null default -1,
	Shadow_Dir	double not null default -1,
	Primary		key(id)
); -- Ships

-- --------------------------------------------------------------------
-- Dredgers work area. At the moment only one is used.
drop table if exists WorkArea;
create table WorkArea (
	-- Row ID
	id		int not null auto_increment,
	-- Last update time.
	Change_Time	datetime not null,
	-- Start point: x-coordinate, meters, local coordinate system.
	Start_X		double not null,
	-- Start point: y-coordinate, meters, local coordinate system.
	Start_Y		double not null,
	-- End point: x-coordinate, meters, local coordinate system.
	End_X		double not null,
	-- End point: y-coordinate, meters, local coordinate system.
	End_Y		double not null,
	-- Forward step size, meters.
	Step_Forward	double not null,
	-- Sideways steps, list of deltas. Example:
	--	-22;-15;-9;-3;3;9;15;22
	Pos_Sideways	char(255) not null,
	Log_Time	datetime not null,
	Primary		key(id)
); -- WorkArea

-- --------------------------------------------------------------------
-- Work log of marking.
drop table if exists WorkLog;
create table WorkLog (
	-- Row ID
	id		int not null auto_increment,
	-- Last update time.
	Change_Time	datetime not null,
	-- ID of a ship who changed this log.
	Ship_Id		int not null,
	-- Index on forward direction, starting from zero.
	Forward_Index	int not null,
	-- Index on sideways direction, starting from zero.
	Sideways_Index	int not null,
	-- Mark, in the range 0..3.
	Mark		int not null,
	Primary		key(id)
);

-- --------------------------------------------------------------------
-- Modembox voltages

DROP TABLE IF EXISTS SupplyVoltages;
CREATE TABLE SupplyVoltages (
    id int NOT NULL auto_increment,
	-- Ship ID of voltage sender
    ShipID int NOT NULL,
	-- Time the reading was recieved
    Reading_Time datetime NOT NULL,
	-- Supply voltage
    SupplyVoltage double NOT NULL,
    PRIMARY KEY  (id)
);

-- --------------------------------------------------------------------
-- Ship and GPS positions
DROP TABLE IF EXISTS ShipPosition;
CREATE TABLE ShipPosition (
    id int not null auto_increment,
	-- When was position inserted to table
    Create_Time datetime not null,
	-- ID of ship whose position it is
    ShipID int NOT NULL,
	-- Type of position
	-- 'M'=MixGPS, 'V'=GpsViewer, '1'=GPS1, '2'=GPS2
    type char(1) not null, 
	-- X, Y and Z coordinates in meters
    x double not null,
    y double not null,
    z double, -- MixGPS and GpsViewer don't have Z coordinate
    -- GPS status, 0 = no fix, 1 = fix, 2 = differential
    GpsStatus int(1)
	-- direction in degrees clockwise from North. Can be null if saving GPS coordinates
    heading double, 
	-- Speed in km/h, can be null if saving GPS coordinates
    speed double,
    PRIMARY KEY (id)
);

-- --------------------------------------------------------------------
-- Weather information for testing page
-- All fields are char(255) as we want to enable user entetring garbage in order to test if server can handle it
DROP TABLE IF EXISTS WeatherInformation;
CREATE TABLE WeatherInformation (
    id int not null auto_increment,
    WindSpeed char(255),    -- Wind speed in m/s
    GustSpeed char(255),    -- Gust speed in m/s
    WindDirection char(255),-- Wind direction in degrees from North
    WaterLevel char(255),   -- Water level in cm
    PRIMARY KEY (id)
);
insert into WeatherInformation set id = 1;