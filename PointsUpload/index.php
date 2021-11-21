<?php
$FILE_ERRORS = array(
        0=>"There is no error, the file uploaded with success",
        1=>"The uploaded file exceeds the upload_max_filesize directive in php.ini",
        2=>"The uploaded file exceeds the MAX_FILE_SIZE directive that was specified in the HTML form",
        3=>"The uploaded file was only partially uploaded",
        4=>"No file was uploaded",
        6=>"Missing a temporary folder"
);

function getLatestPointfile()
{
	if (!$dir = opendir('.')) {
		return 'Error: directory not found';
	}
	$latest = -1;
	while (($file = readdir($dir)) !== false){
		if($file != "." && $file != "..") {
			$pieces1 = explode("_", $file);
			$pieces2 = explode(".", $pieces1[1]);
			$num = intval($pieces2[0]);
			$latest = max($latest, $num);
		}
	}
	closedir($dir);
	return $latest;
}

$submitted = $_GET["submit"];

if (isset($submitted)) {
	if ($_FILES["file"]["error"] > 0) {
		echo "Error: " . $FILE_ERRORS[$_FILES["file"]["error"]] . "<br />";
		echo '<a href="index.php">Back</a>';
	} else {
		if ($_FILES["file"]["size"]>"4194303"){
			$size = sprintf("%2.2f", $_FILES["file"]["size"]/1024/1024);
			echo "Error: File too big ($size MB). Maximum size is 4 MB<br />";
			echo '<a href="index.php">Back</a>';
		} else {
			$seq = getLatestPointfile() + 1;
			$filename = sprintf("points_%05d.ipt", $seq);
			move_uploaded_file($_FILES["file"]["tmp_name"], "./".$filename);
			chmod("./$filename", 0644);
			echo "File saved as $filename <br />";
			echo '<a href="index.php">Back</a>';
		}
    	}
} else {
	echo '<html>
<body>
<p>';
        $latest = getLatestPointfile();
        if ($latest !== false){
		printf("Latest file was with sequence number %05s\n",   $latest);
        }
	echo '</p>
<p>Maximum size is 4MB</p>
<form action="index.php?submit=1" method="post"
enctype="multipart/form-data">
<label for="file">Filename:</label>
<input type="file" name="file" id="file" /> 
<input type="hidden" name="MAX_FILE_SIZE" value="4194303" />
<br />
<input type="submit" name="submit" value="Submit" />
</form>

</body>
</html>';
}
?>
