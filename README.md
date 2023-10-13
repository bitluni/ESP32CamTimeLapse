# ESP32CamTimeLapse

Please visit https://bitluni.net/esp32camtimelapse for project information.

## Create timelapse

`ffmpeg -r 60 -f image2 -i pic%05d.jpg -vcodec libx264 -crf 10 -pix_fmt yuv420p lapse.mp4`

The meaning of the parameters is:

-r frames per second in the final video
-f image processor (ignore that)
-i input files. %05d means if will fill in 00000 to 99999 there. (you can add the parameter -start_number 69 to start from 00069)
-crf quality. lower is better. 10 is already like 80MBs for the highest resolution
-pix_fmt sets the pixel format for mp4 (ignore that)
