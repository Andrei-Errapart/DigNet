<?php

// Database information
$host = 'localhost';
$user = 'dignet';
$pass = 'peeterkalleandrei';
$dbname = 'DigNet';

$conn = mysql_connect($host, $user, $pass) or die ('Error opening DB connection');
mysql_select_db($dbname);


function getValue($name) {
	$param = mysql_real_escape_string($_POST[$name]);
	if (strcmp($param, $_POST[$name]) != 0) {
		unset($param);
	}
	return $param;
}

class WeatherInfo {
	public $windSpeed;
	public $gustSpeed;
	public $windDirection;
	public $waterLevel;
	public $hasData;

	function setHasData($var){
		if (isset($windSpeed)){
			$this->hasData = true;
		}
	}
	function dataUpdated() {
		$this->hasData = false;
		$this->setHasData($this->windSpeed);
		$this->setHasData($this->gustSpeed);
		$this->setHasData($this->windDirection);
		$this->setHasData($this->waterLevel);
	}

	function toString() {
		return 'Wind speed: '.$this->windSpeed.' Gust speed: '.$this->gustSpeed.' WindDirection: '.$this->windDirection.' Water level: '.$this->waterLevel;
	}

	function fromDb() {
		$getWeatherQuery = "select WindSpeed, GustSpeed, WindDirection, WaterLevel from WeatherInformation where id = 1";
		$result = mysql_query($getWeatherQuery) or die('<p>Cannot read weather informaton from DB');
		
		$info = mysql_fetch_array($result, MYSQL_ASSOC);
		if (isset($info)){
			$this->windSpeed = 	$info['WindSpeed'];
			$this->gustSpeed = 	$info['GustSpeed'];
			$this->windDirection = 	$info['WindDirection'];
			$this->waterLevel = 	$info['WaterLevel'];
			$this->dataUpdated();
		} else {
			$this->hasData = false;
		}
		return $this->hasData;
	}
	function fromPost() {
		$this->windSpeed = 	getvalue('windSpeed');
		$this->gustSpeed = 	getvalue('gustSpeed');
		$this->windDirection = 	getvalue('windDirection');
		$this->waterLevel = 	getvalue('waterLevel');
		$this->dataUpdated();
		return $this->hasData;
	}
	function saveToDb() {
		$insert = '
			UPDATE WeatherInformation SET
				WindSpeed 	= \''.$this->windSpeed.'\', 
				GustSpeed 	= \''.$this->gustSpeed.'\', 
				WindDirection	= \''.$this->windDirection.'\', 
				WaterLevel 	= \''.$this->waterLevel.'\'
			WHERE id = 1';
		mysql_query($insert) or die('#error can\'t update weather information information: '.$insert);
	}
};

function createForm($info){
	echo '<html>
<body>
<h1>Weather data input</h1>
<form action="admin.php?submit=1" method="post"
enctype="multipart/form-data">
<table>
<tr>
	<td><label for="windSpeed">Wind speed:</label></td>
	<td><input type="text" name="windSpeed" id="windSpeed" value="'.$info->windSpeed.'" /> </td>
</tr>
<tr>
	<td><label for="gustSpeed">Gust speed:</label>
	<td><input type="text" name="gustSpeed" id="gustSpeed" value="'.$info->gustSpeed.'" /></td>
</tr>
<tr>
	<td><label for="windDirection">Wind direction:</label>
	<td><input type="text" name="windDirection" id="windDirection" value="'.$info->windDirection.'" /></td>
</tr>
<tr>
	<td><label for="waterLevel">Water level:</label>
	<td><input type="text" name="waterLevel" id="waterLevel" value="'.$info->waterLevel.'" /></td>
</tr>
<tr>
	<td><input type="submit" name="submit" value="Submit" /></td>
</tr>
</table>
</form>

</body>
</html>';
}

// Parameters from user
if (isset($_GET['submit'])){
	$getWeather = new WeatherInfo();
	$getWeather->fromPost();
	$getWeather->saveToDb();
}
$dbWeather = new WeatherInfo();
$dbWeather->fromDb();

createForm($dbWeather);
echo 'done';
mysql_close($conn);
