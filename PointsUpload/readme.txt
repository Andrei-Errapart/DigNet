Usage:
Copy index.php and .htaccess into a directory
cd to the created directory
create a user:
# htpasswd -c /path/to/directory/.htpasswd username
Add writing permissions for the folder to www-data

All the files being uploaded will go into the same directory with names 
of points_XXXXX.ipt where XXXXX is one bigger than previous biggest. If 
previous is 2^16 then things will go bad
