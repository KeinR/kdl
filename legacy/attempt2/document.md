
Practical Web Scraping and Data Reporting Language
(or something to that effect)


Use case:
- Keeping and updating data records that must synchronize with publicly 
accessible website data.

Examples:
- Keeping track of what anime you have watched
- Comparing you and your friend's steam libraries for commonalities
- Regularly scraping a webpage for chapters of a particular fanfic that takes 
too long to update (*cough cough* To The Stars) and performing some operation on 
the extracted text (creating a PDF, pretty formatting it in a text file, etc.)
- Archiving web content, to some extent

Notes:
- For safety and simplicity, will never run JavaScript

I dare say a library would be better for this but whatever.

Syntax:

```

|| -> read/write
>> -> read
<< -> write

web <web page> ? param_1 ; param_2 ; regex ; ( [<regex>] | <<selector>> ) >> $x
sql <sqlite_file> ? str [SELECT * FROM ? WHERE shoes=false] ; "foobar" >> $x
x ? "SELECT shoes FROM foobar" >> y

sql <sqlite_file> || $z
z ? ...



none 42352 >> $p

get https://myanimelist.net/anime/*/ ? p ; sel [div.di-ib:nth-child(6) > span:nth-child(2)] >> x

# FAil checks the failure of the last command
fail [Request failed: ?] ? x
val x ? [^\d{1,4}$] ; [Error: anime failed to validate: ? ] $p; 

*sql mydatabase.sql ? [INSERT INTO anime VALUES (?, ?)] ; $p ; $x ; 

make array ? 4235 ; 442 ; [I like cake] >> %array

math [$x + 5] >> $sss

begin ifname iterate [$x = $y and %array:2 ]
 
end ifname

...

Later...

...



# The default proc called when none is specified on the command line
proc @main

# $0 is first argument, $1 is second, etc...

*sql mydatabase.sql ? [UPDATE anime SET watched =  WHERE id=?] ; $p >> $result
fail [Database query failed]
out [Update success]

endproc @main


```


Funcitons that have afteraffects must be prefixed by an asterisk.

Otherwise, they will not be perminent.

There are two temporary sections: the volatile and the temp.
Volatile is the place where stuff without afteraffects goes.
Temp is where stuff with afteraffects goes.
Temp is flushed to the real filesystem after the program ends execution with no errors.

Program terminates on any errors


