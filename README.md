# Feather TFT LoRa Sniffer
Easy to build device that can scan LoRa channels, displaying any monitored packets on the TFT display while logging data to an SD card.

![Device view](https://github.com/ImprobableStudios/Feather_TFT_LoRa_Sniffer/blob/master/img/Overview.JPG "Device view")

## Parts
- [Adafruit Feather M0 with RFM95 LoRa Radio - 900MHz](https://www.adafruit.com/product/3178)
- [Adafruit TFT FeatherWing](https://www.adafruit.com/product/3315)
- [Lithium Ion Polymer Battery - 3.7v 500mAh](https://www.adafruit.com/product/1578)
- [900Mhz Antenna Kit](https://www.adafruit.com/product/3340)
- [3D Printed Case for TFT FeatherWing](https://learn.adafruit.com/3d-printed-case-for-adafruit-feather/tft-feather-wing)
- [uFL SMT Antenna Connector](https://www.adafruit.com/product/1661) - Have a few of these on-hand, they can be a challenge to solder
- 4x 2.5M machine screws
- [Slide Switch](https://www.adafruit.com/product/805)

## Tools
- 3D Printer (FlashForge Creator Pro used, with non-pigmented ABS filament)
- Screwdriver
- Hot-glue gun
- Drill and bits

## Software and Libraries
- [Arduino IDE](https://www.arduino.cc/en/Main/Software) or [Visual Micro plugin for Microsoft Visual Studio](http://www.visualmicro.com/)
- [SD library](https://www.arduino.cc/en/Reference/SD)
- [Adafruit_GFX library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit_ILI9341 library](https://github.com/adafruit/Adafruit_ILI9341)
- [RadioHead library](http://www.airspayce.com/mikem/arduino/RadioHead/)

## Introduction
When I first read about [LoRa](https://www.postscapes.com/long-range-wireless-iot-protocol-lora/), I was reminded of using [Packet Radio](https://en.wikipedia.org/wiki/Packet_radio) with my dad when I was young.  Back then, things were a bit different, there wasn't an internet connected smartphone with unlimited SMS in every pocket for communications. Talking to others digitally meant either using a terminal connected to a TNC for Packet Radio or dialing in to a BBS. With the ease of communication everyone takes for granted today, it's easy to miss just how cool and exciting this tech really is.

Many commercial providers are starting to bring up LoRaWAN networks around the world, some even offering access for around [$1/per device/per year](https://finance.yahoo.com/news/unabiz-announces-iot-connectivity-1-023200301.html). However, because this tech uses the unlicensed ISM bands, it is accessible to all.  Anyone can set up their own LoRaWAN gateway with access to the internet. [The Things Network](https://www.thethingsnetwork.org/) is a great example of people coming together to build a free network using LoRa. There are many great reasons to make use of this for IoT, it doesn't take long to look around at potential projects and become inspired.

My interests are a bit different; the [RadioHead library](http://www.airspayce.com/mikem/arduino/RadioHead/) also supports setting up mesh networks.  This is much more like the Packet Radio I grew up with.  Each node can run without a connection to the internet and acts as a "repeater", helping packets get from one side of the network to the other. All of the free-standing nodes in the mesh will work together to get your data to where it needs to go. You could run this network with no connection to the internet at all, or perhaps with a small server acting as an exit node on one end, allowing access to the data from all of your nodes. 

Before building a mesh with a bunch of nodes, I decided it would be interesting to see how much LoRa is being used in my area. How is this best achieved? By building a portable sniffer/scanner of course! This device should be in a form factor that I can set on a windowsill, put in my pocket when I walk around, or on my dash while I drive.  

When I saw Adafruit's [3D printable case](https://learn.adafruit.com/3d-printed-case-for-adafruit-feather/tft-feather-wing) for their [TFT FeatherWing](https://www.adafruit.com/product/3315), I was intriqued.  This looked like the perfect enclosure for my project. This, along with a [Feather M0 with RFM95 LoRa Radio](https://www.adafruit.com/product/3178), an antenna, battery, and switch makes a complete solution.

## Assembly
I am going to recycle some bits and suggest you take a look at [Adafruit's how-to for the TFT FeatherWing enclosure](https://learn.adafruit.com/3d-printed-case-for-adafruit-feather/tft-feather-wing). They do a great job of showing how to put it together.

![Parts view](https://github.com/ImprobableStudios/Feather_TFT_LoRa_Sniffer/blob/master/img/CaseOpen.JPG "Parts view")

The only challenge I ran into in assembly was soldering the uFL connector for the antenna.  This was difficult without my microscope (even a USB camera microscope would have helped), but I managed to do it, only damaging one connector in the process. The trick is to prime one ground pad with solder, then work the connector into place while heating the pad.  Once that is done, the other two pads can be soldered as well.  Take care when soldering the center tab, as it is easy to short this to ground (I did this my first attempt). **Check for (lack of) continuity between the center and ground after you finish to be sure the antenna is not shorted**.

![uFL view](https://github.com/ImprobableStudios/Feather_TFT_LoRa_Sniffer/blob/master/img/uFL.JPG "uFL view")

I also needed to find a place for the external antenna connector in the case.  I found that the lower left corner was the best spot, just to the left of the SD-card slot. Even with the provided lock-washer, it was necessary to secure the connector to prevent it from rotating.  I used hot-glue to do this, but superglue, or acetone-welding would have likely worked just as well.

![Antenna view](https://github.com/ImprobableStudios/Feather_TFT_LoRa_Sniffer/blob/master/img/Antenna.JPG "Antenna view")

To secure the battery, I used a small piece of PSA (double stick tape). This fits nicely under the SD card slot, but may need to be angled in order to fit well between the antenna connector and switch. **I strongly suggest doing this last and with care as you do not want to bend or puncture the battery**.

Now that we have a device, let's take a look at making it do something useful.

## Firmware
When Adafruit introduced the Cortex-M0 based Feathers, I was pretty excited.  I have a lot of experience working "on-the-metal" with the Cortex-M series in commercial products including helping architect the initial OS/scheduler used in the Xbox One Controller (Cortex-M0) and in the now "disbanded" Microsoft Band (Cortex-M4).  Currently I am at a dark-startup where we are using an nRF51 and Kinetis K24 with FreeRTOS, both also from the Cortex-M family. 

Working directly on-the-metal with nothing in your way except the header files and compiler can be fun and educational, but sometimes you just want to sit down and make something cool. Using an Arduino compatible IDE with the Feather M0 is a great way to do this.

I spend most of my professional time in either Visual Studio or Keil, so I find the Arduino IDE a little too basic. I opted to use the [Visual Micro plugin for Microsoft Visual Studio](http://www.visualmicro.com/). That's why you will see files in the project besides `Feather_TFT_LoRa_Sniffer.ino`. Really, that's the only file you need if you use the Arduino IDE and have the libraries listed above installed.

The INO file is well commented and should allow you to jump in and understand what is going on without too much difficulty. But, there are a few things I will clarify here.

### Text Scrolling
The Adafruit GFX library doesn't have support for scrolling text on the screen, and that's what I wanted this device to do, so I built a function to handle this:

```c
void tft_printfln(uint8_t size, uint16_t fg_color, uint16_t bg_color, char *fmt, ...)
{
    static uint16_t line = 0;
    static bool scroll = false;

    // Resulting string limited to PRINTFLN_MAXLEN chars
    char buf[PRINTFLN_MAXLEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, PRINTFLN_MAXLEN, fmt, args);
    va_end(args);

    _tft.setTextColor(fg_color, bg_color);
    _tft.setTextSize(size);

    // Handle repositioning in the framebuffer
    if (line >= _tft.height() / TEXT_HEIGHT_UNSCALED)
    {
        scroll = true;
        line = 0;
        _tft.setCursor(0, 0);
    }

    if (scroll)
    {
        _tft.fillRect(0, (TEXT_HEIGHT_UNSCALED * line), _tft.width(), (TEXT_HEIGHT_UNSCALED * size), bg_color);
        _tft.scrollTo((TEXT_HEIGHT_UNSCALED * line) + (TEXT_HEIGHT_UNSCALED * size));
    }

    _tft.println(buf);
    line += size;
}
```

There is a bug in the function that causes it to occasionally cut off a line when scrolling, but I haven't had a chance to track this down yet. Regardless, it's pretty cool to be able to write scrolling text to this little screen with ease.

![Scrolling view](https://github.com/ImprobableStudios/Feather_TFT_LoRa_Sniffer/blob/master/img/Scrolling.JPG "Scrolling view")

### Frequency Scanning
The code below is used to define what frequencies should be monitored, and for how long. The frequencies I chose are the US primary up and down links for [The Things Network](https://www.thethingsnetwork.org/wiki/LoRaWAN/Frequencies/Frequency-Plans#lorawan-frequencies_us902-928), as well as the frequencies mentioned on [Adafruit Learn](https://learn.adafruit.com/adafruit-feather-m0-radio-with-lora-radio-module/overview?view=all) for the LoRa module.

```c
// Define the frequencies to scan
#define FREQ_COUNT 19
float _frequencies[] =
{
    868.0, 915.0,                                                   // From Adafruit Learn
    903.9, 904.1, 904.3, 904.5, 904.7, 904.9, 905.1, 905.3, 904.6,  // TTN - US Uplink
    923.3, 923.9, 924.5, 925.1, 925.7, 926.3, 926.9, 927.5          // TTN - US Downlink
};

// How long should a frequency be monitored
#define FREQ_TIME_MS            5000
```

There is likely some optimization that can be done around this. The radio can only monitor one frequency at a time, and you must receive an entire LoRa packet while listening in order to parse it - but, the more time you spend on one channel, the more you might be missing on others! Currently I have this set at 5 seconds going across 19 different channels. This seems to work ok, I am seeing traffic for sure.

When changing frequencies, the display will show what frequency it is monitoring and the number of valid packets that have been previously seen on that frequency.  Here you can see that I picked up quite a bit of data just by leaving the device running while sitting next to a window for a few days.

![Scanning view](https://github.com/ImprobableStudios/Feather_TFT_LoRa_Sniffer/blob/master/img/Scanning.JPG "Scanning view")

### Logging to SD
If you have an SD card installed when you power on the device, it will log the data received in CSV format for you. The last column of data is a raw print of the bytes received.  In general, this isn't useful, but in the case of someone sending plain-text, you might just find something!

> Note: The FAT filesystem... sucks. I have worked with it for years, even writing my own embedded implementation for commercial projects. Yes, it is compatible with everything, but it is incredibly easy to corrupt the File Allocation Table and render the filesystem unreadable. Just keep this in mind. You may lose data if you pull the card out while a write is happening, or if you fail to flush at the right interval. Consider yourself warned.

Here is a view of some data received just by leaving the device running for a day. `DataBytes` is cut off in the picture, but the column contains all the bytes received. The ASCII display of the data wasn't useful in this case, so I clipped it from the image.

![Excel view](https://github.com/ImprobableStudios/Feather_TFT_LoRa_Sniffer/blob/master/img/Excel.jpg "Excel view")

## Conclusion
Hopefully you find this project useful. Let me know what you do with it and how you have modified it. I'm always open to suggestions and ideas!

My next project will likely be a standalone, solar charged mesh node that sends its' battery level back to another node.  This will allow me to see how well a true wireless node can perform in "sunny" Seattle.

Thanks for reading!

-Joe
(joe at improbablestudios.com)

