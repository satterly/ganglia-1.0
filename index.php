<?
/*
 * "Copyright (c) 2000 by Matt Massie and The Regents of the University
 * of California.  All rights reserved."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */                        

/* $Id: index.php,v 1.1 2000/11/03 18:06:56 massie Exp massie $ */

/* Variables to set */

                   /* Change this to IP of your axon */
$axon_address    = "xxx.xxx.xxx.xxx";
$axon_port       = "9290";
$axon_timeout    = 10;  /*seconds*/
$refresh         = 15;  /*seconds*/

$header_color    = "#FFFFCC";
$table_border    = "#444444";
$head_tail_color = "#DODOFF";
$line_color      = "#CCCCCC";
$title           = "Ganglia Cluster Monitoring System";
$graph_height    = 120;
$graph_width     = 360;
$max_tokens      = 5;

/* Nothing needs to be changed from here down */

header ("Expires: Mon, 26 Jul 1997 05:00:00 GMT");    // Date in the past
header ("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT"); // always modified
header ("Cache-Control: no-cache, must-revalidate");  // HTTP/1.1
header ("Pragma: no-cache");                          // HTTP/1.0 
?>
<HTML>
<HEAD>
<TITLE><? echo $title;?></TITLE>
<META http-equiv="refresh" 
         content="<?echo $refresh;?> URL=<?print "$PHP_SELF?$QUERY_STRING";?>">
<META http-equiv="Expire"  
         content="now">
</HEAD>
<BODY BGCOLOR="#FFFFFF">

<?

$valid_tokens = array (
    "cpu_num"      => array(
                       "menu"  => "Number CPUs",
                       "units" => ""),
    "cpu_speed"    => array(
                       "menu"  => "CPU Speed",
                       "units" => "MHz"),
    "cpu_user"     => array(
                       "menu"  => "CPU User",
                       "units" => "%"),
    "cpu_nice"     => array(
                       "menu"  => "CPU Nice",
                       "units" => "%"),
    "cpu_system"   => array(
                       "menu"  => "CPU System",
                       "units" => "%"),
    "cpu_idle"     => array(
                       "menu"  => "CPU Idle",
                       "units" => "%"),
    "cpu_aidle"    => array(
                       "menu"  => "CPU Abs. Idle",
                       "units" => "%"),
    "load_one"     => array(
                       "menu"  => "Load (1 min.)",
                       "units" => ""),
    "load_five"    => array(
                       "menu"  => "Load (5 min.)",
                       "units" => ""),
    "load_fifteen" => array(
                       "menu"  => "Load (15 min.)",
                       "units" => ""),
    "proc_run"     => array(
                       "menu"  => "Running Procs",
                       "units" => ""),
    "proc_total"   => array(
                       "menu"  => "Total Procs",
                       "units" => ""),
    "rexec_up"     => array(
                       "menu"  => "rEXEC up?",
                       "units" => ""),
    "ganglia_up"   => array(
                       "menu"  => "Ganglia up?",
                       "units" => ""),
    "mem_total"    => array(
                       "menu"  => "Mem Total",
                       "units" => "kB"),
    "mem_free"     => array(
                       "menu"  => "Mem Free",
                       "units" => "kB"),
    "mem_shared"   => array(
                       "menu"  => "Mem Shared",
                       "units" => "kB"),
    "mem_buffers"  => array(
                       "menu"  => "Mem Buffers",
                       "units" => "kB"),
    "mem_cached"   => array(
                       "menu"  => "Mem Cached",
                       "units" => "kB"),
    "swap_total"   => array(
                       "menu"  => "Swap Total",
                       "units" => "kB"),
    "swap_free"    => array(
                       "menu"  => "Swap Free",
                       "units" => "kB"));

?>
<FORM METHOD="GET" NAME="GANGLIA_FORM">
<TABLE BORDER="0" CELLPADDING="1" CELLSPACING="0" 
       BGCOLOR="<?echo $table_border;?>" WIDTH="100%">
<TR><TD>
<TABLE WIDTH="100%" BORDER="0" BGCOLOR="#FFFFFF">
   <TR>
      <TD BGCOLOR="<?echo $head_tail_color;?>"
          VALIGN="TOP">
      <B>
      <?$time = time();
        echo "<PRE>\n";
        echo date("m/d/Y",   $time);
        echo "\n";
        echo date("h:i:s a", $time);
        echo "\n";
        echo "</PRE>\n";
        print "Refreshes in $refresh seconds"; 
      ?>
      
      </B>
      </TD>
      <TD COLSPAN="<?$c=$max_tokens;echo $c;?>" 
          BGCOLOR="<?echo $head_tail_color;?>">
      <FONT SIZE="+3">
      <IMG SRC="graph.php?height=<?echo $graph_height;?>&width=<?echo $graph_width;?>" 
           HEIGHT="<?echo $graph_height;?>"
           WIDTH ="<?echo $graph_width;?>">
      </FONT>
      </TD>
   </TR>
   <TR>
   <TD ALIGN="CENTER"
       BGCOLOR="<?echo $header_color;?>"><B>Cluster Node</B></TD>
<?
for ($i = 0 ; $i < $max_tokens ; $i++ ){
   print "<TD BGCOLOR=\"$header_color\"><B>";
   if ( !$i ){
      print "Sort by...";
   } else {
      print "then by...";
   }
   print "</B></TD>"; 
}
?>
   </TR>
   <TR>
      <TD ALIGN="LEFT">
      <B>Filter: </B><INPUT onBlur="GANGLIA_FORM.submit();"
                                TYPE="TEXT" NAME="regex" SIZE="10"
      <? if ( $regex ){
             print " VALUE=\"$regex\" ";
         }
      ?>
      ><BR>
      <B>Nodes: </B><INPUT onBlur="GANGLIA_FORM.submit();"
                               TYPE="TEXT" NAME="num_nodes" SIZE="4" 
      <? if ( $num_nodes ){
           print "VALUE=\"$num_nodes\"";
         } else {
           print "VALUE=\"0\"";
         }
         print ">";
      ?>
      </TD> 
<?

for ($i = 0 ; $i < $max_tokens ; $i++ ){

    print "<TD>\n";
    print "<SELECT NAME=\"token_choice[$i]\" onChange=\"GANGLIA_FORM.submit();\">\n";
    $j=$i-1;
    print "<OPTION VALUE=\"0\">none</OPTION>";
    while (list ($key, $val) = each ($valid_tokens)) {
       print "<OPTION VALUE=\"$key\"";
       /* If they choose, "none" all following become "none" too */ 
       if ( ($token_choice[$i] == $key) && 
            ($token_choice[0]  != "0")){
          print " SELECTED";
       }
       print ">$val[menu]</OPTION>\n";
    }
    reset($valid_tokens);
    print "</SELECT><BR>\n";
    print "<INPUT onClick=\"GANGLIA_FORM.submit();\"
                  TYPE=\"RADIO\" NAME=\"order_choice[$i]\" VALUE=\"-1\" ";
    if ( ($order_choice[$i] == -1 ) || !$order_choice[$i] ){
       print "CHECKED"; 
    }
    print ">descend ";
    print "<INPUT onClick=\"GANGLIA_FORM.submit();\"
                  TYPE=\"RADIO\" NAME=\"order_choice[$i]\" VALUE=\"1\" ";
    if ( $order_choice[$i] == 1 ){
       print "CHECKED";
    } 
    print ">ascend</TD>\n";
   
}
print "</TR>\n";
print "</FORM>\n";

/* If we don't have any data just stop here */
if (! isset($num_nodes) ){
   print "</TABLE></TD></TR></TABLE>\n";
   include "tail.php";
   exit;
}

/* How many nodes do you want data for? */
$query = pack("n", $num_nodes);

$i = 0;
while ( $token_choice[$i] ) {
   $query.= pack ("a16n", $token_choice[$i], $order_choice[$i] ); 
   $i++;
}

$fp = fsockopen($axon_address, $axon_port , &$errno, &$errstr, $axon_timeout);

if(!$fp) {

   echo "$errstr ($errno)<br>\n";

   } else {

        fwrite ( $fp, $query ); 
        $line_num = 0;

        while(!feof($fp)) {

            /* Strip off the leading white space */
            $line = ereg_replace ("^ *", "", fgets($fp, 512) ); 

            /* Skip empty lines */
            if (! $line ){ continue; }

            /* Explode the tab-delimited data */
            $return_data = preg_split( "/\s+/", $line );

            /* Check regular expressions */
            if ( $regex ){
               if (! ereg( $regex, $return_data[0] ) ){
                   continue;
               }
            }
            print "<TR>\n";

            for ($i = 0; $i <= $max_tokens ; $i++ ){

                print "<TD ";
                if (! ($line_num % 2) ){
                     print "BGCOLOR=\"$line_color\"";
                }
                print ">"; 
                if ( strlen( $return_data[$i]) ){
                     print "<FONT SIZE=\"+1\">\n";
                     print "$return_data[$i] "; 
                     /* Print units for columns */
                     if( $i ){
                           $j = $i - 1;
                           $j = $token_choice[$j];
                           $j = $valid_tokens[$j];
                           print "$j[units]\n"; 
                     }
                     print "</FONT>\n";
                } else {
                     print "&nbsp;";
                }
                print "</TD>\n";

                /* Initialize to zero */
                if ( ! $i ){ $total[$i]=0; }

                /* Sum up the returned data */
                $total[$i]+=$return_data[$i];
            }
            print "</TR>\n";
            $line_num++; 
        }
   fclose($fp);
}

if( ! $line_num ) {
  print "</TABLE></TD></TR></TABLE>\n";
  include "tail.php";
  exit;
}

/* Print averages */
print "<TR>\n";

for ($i = 0 ; $i <= $max_tokens ; $i++ ){

   print "<TD BGCOLOR=\"$head_tail_color\" ";
   if ( ! $i ){
       print "ALIGN=\"RIGHT\">";
       print "<FONT SIZE=\"+1\">Average:</FONT>";
   } else {
       if ( $total[$i] ){
          $avg = sprintf ("%.2f", ( $total[$i]/$line_num ) );
          print "><FONT SIZE=\"+1\">$avg</FONT>";
       } else {
          print ">&nbsp;";
       }
   }
   print "</TD>\n";
}
print "</TR>\n";

/* Print totals */
print "<TR>\n";

for ($i = 0 ; $i <= $max_tokens ; $i++ ){

   print "<TD BGCOLOR=\"$head_tail_color\" ";
   if ( ! $i ){
       print "ALIGN=\"RIGHT\">";
       print "<FONT SIZE=\"+1\">Total:</FONT>";
   } else {
       if ( $total[$i] ){
          print "><FONT SIZE=\"+1\">$total[$i]</FONT>"; 
       } else {
          print ">&nbsp;";
       }
   }
   print "</TD>\n";
}
?>    
</TR>
</TABLE>
</TD></TR></TABLE>
</BODY>
</HTML>
