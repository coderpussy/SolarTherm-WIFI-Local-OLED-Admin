<?php
    // Get json stream content
    $json = file_get_contents('php://input');
        
    if(!empty($json)) {
        // Decode json string
        $obj = json_decode($json);

        // If json is valid
        if(json_last_error() === JSON_ERROR_NONE) {

            // Get device
            $device = '';
            if(!empty($obj->device)){
                $device = $obj->device;
            }
            
            // Connect to database
            $servername = "localhost";
            $username = "solartherm";
            $password = "solartherm";
            $dbname = "solartherm";

            // Create connection
            $conn = new mysqli($servername, $username, $password, $dbname);
            
            // Check connection
            if ($conn->connect_error) {
                die("Database Connection failed: " . $conn->connect_error);
            }

            // mysql select query string
            $sql = 'SELECT `TimeStamp` FROM `sensors` WHERE `name`="' . $device . '" AND `TimeStamp` >= NOW() - INTERVAL 1 HOUR ORDER BY TimeStamp DESC LIMIT 1';

            // execute query
            $result = $conn->query($sql);

            // fetch query result
            $row = '';
            if ($result->num_rows > 0) {
                // fetch data row
                $row = $result->fetch_row();

                // set timezone
                date_default_timezone_set('Europe/Berlin');
                
                // get timespan in seconds
                $start = strtotime($row[0]);
                $end = strtotime(date("Y-m-d H:i:s"));
                $secs = round($end - $start);

                // echo seconds for payload
                echo $secs;
            } else {
                // echo error 0 seconds
                echo 0;
            }

            $conn->close();
        }
    }
?>