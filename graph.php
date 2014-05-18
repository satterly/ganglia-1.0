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

/* $height and $width are set by GETs from index.php */

                   /* Set to the address of your Axon */
$axon_address    = "xxx.xxx.xxx.xxx";
$axon_port       = "9290";
$axon_timeout    = 10;  /*seconds*/
  
/********************************************************/
/* You shouldn't need to change anything from here down */
/********************************************************/

/*Make sure the image is not cached*/
header ("Expires: Mon, 26 Jul 1997 05:00:00 GMT");    // Date in the past
header ("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT"); // always modified
header ("Cache-Control: no-cache, must-revalidate");  // HTTP/1.1
header ("Pragma: no-cache");                          // HTTP/1.0
# For debugging
#header ("Content-type: text/html");
header ("Content-type: image/gif");

/* Create the image space to write to */
$im = @ImageCreate ($width, $height)
            or die ("Cannot Initialize new GD image stream");
$background_color   = ImageColorAllocate ($im, 255, 255, 255);
$black              = ImageColorAllocate ($im, 0,   0,   0); 
imagecolortransparent( $im, $background_color );

$fp = fsockopen($axon_address, $axon_port , &$errno, &$errstr, $axon_timeout);

if(!$fp) {
 
   ImageString($im, 1, 5, 5, "$errstr ($errno)", $black);
   ImageGif ($im);
   exit;
 
} 

fwrite ( $fp, pack ("na16na16na16na16na16na16na16na16n", 
                                   0,
                        "cpu_num", 1,
                       "cpu_user", 1,
                       "cpu_nice", 1,
                     "cpu_system", 1,
                       "cpu_idle", 1,
                      "mem_total", 1,
                       "mem_free", 1,
                    "mem_buffers", 1 )); 

$total[machines]   = 0;
$total[cpu_num]    = 0;
$total[cpu_user]   = 0;
$total[cpu_system] = 0;
$total[cpu_ridle]  = 0;
$total[mem_total]  = 0;
$total[mem_free]   = 0;
$total[mem_buffers]= 0; 

while(!feof($fp)) {

   /* Strip off the leading white space */
   $line = ereg_replace ("^ *", "", fgets($fp, 512) );

   /* Skip empty lines */
   if (! $line ){ continue; }             

   /* Explode the tab-delimited data */
   $return_data = preg_split( "/\s+/", $line ); 

   $total[cpu_num]    += $return_data[1];

   $total[cpu_user]   += ($return_data[1] * $return_data[2]);
   $total[cpu_nice]   += ($return_data[1] * $return_data[3]);
   $total[cpu_system] += ($return_data[1] * $return_data[4]);

   /* I NEED TO FIX THIS BUG SOON.. 0 gives me 65535 */
   if ( $return_data[5] > 100 || $return_data[1] < 0 ){
	$return_data[5]=0;
   }
   $total[cpu_ridle]  += ($return_data[1] * $return_data[5]);

   $total[mem_total]  += $return_data[6];
   $total[mem_free]   += $return_data[7];
   $total[mem_buffers]+= $return_data[8];
   
   $total[machines]++;
}
 


$text_color         = ImageColorAllocate ($im, 233, 14,  91);
$graph_border_color = ImageColorAllocate ($im, 0,   0,   0);

$final[cpu_user]   = sprintf ("%.1f", ($total[cpu_user]  /$total[cpu_num]) );
$final[cpu_nice]   = sprintf ("%.1f", $total[cpu_nice]  /$total[cpu_num] );
$final[cpu_system] = sprintf ("%.1f", $total[cpu_system]/$total[cpu_num] );
$final[cpu_ridle]  = sprintf ("%.1f", $total[cpu_ridle] /$total[cpu_num] );
$final[mem_free]   = sprintf ("%.1f",100-(($total[mem_free] + $total[mem_buffers]) /
                                      $total[mem_total] * 100));

$memstring = sprintf("%.1f GB", (($total[mem_total]/1024)/1024) );
imagestring( $im, 5, 90, 0, "$total[cpu_num] CPUs", $black );
imagestring( $im, 5, 270, 0, $memstring, $black );
imagestring( $im, 2, 35, $height-15, "%user", $black );
imagestring( $im, 2, 85, $height-15, "%nice", $black );
imagestring( $im, 2, 131,$height-15, "%system", $black );
imagestring( $im, 2, 185,$height-15, "%idle", $black );
imagestring( $im, 2, 274,$height-15, "%mem used", $black );

percent_bar( 50,  $height-15, 50,  15,  $final[cpu_user] );
percent_bar( 100, $height-15, 100, 15,  $final[cpu_nice] );
percent_bar( 150, $height-15, 150, 15,  $final[cpu_system] );
percent_bar( 200, $height-15, 200, 15,  $final[cpu_ridle] ); 
percent_bar( 300, $height-15, 300, 15,  $final[mem_free] );




function percent_bar ( $start_x, $start_y, $end_x, $end_y, $percent ) {
   global $im, $text_color, $black;
  
   $bar_width = 6;
   $font_size = 3;
   $font_width  = imagefontwidth ( $font_size );
   $font_height = imagefontheight( $font_size );

   imageline( $im, $start_x, $start_y, $end_x, $end_y, $black );
   imageline( $im, $start_x- $bar_width, $start_y,
                   $start_x+ $bar_width, $start_y, $black );
   imageline( $im, $end_x  - $bar_width, $end_y,
                   $end_x  + $bar_width, $end_y,   $black );

   $percent_y = $start_y - abs($start_y - $end_y) * ( $percent / 100 );

   imagestring( $im, $font_size, 
                     $start_x - strlen($percent)* $font_width, 
                     $percent_y - $font_height, 
                     "$percent",  $black);

   $points[0]=$start_x;
   $points[1]=$percent_y;
   $points[2]=$points[0]+$bar_width;
   $points[3]=$points[1]-$bar_width;
   $points[4]=$points[0]+$bar_width;
   $points[5]=$points[1]+$bar_width;

   imagefilledpolygon ($im, $points, 3,  $text_color) ;

}




ImageGif ($im);

?>
