/*
    Psuedo code from www.easyrgb.com/en/math.php ported to C and released
    under MIT license with permission.  See below.
*/

/*
    Copyright 2019 Bryan Christ

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/*
    Delivered-To: bryan.christ@gmail.com
    Received: by 2002:a19:998d:0:0:0:0:0 with SMTP id b135csp3556509lfe;
    Sun, 13 Jan 2019 23:57:15 -0800 (PST)

    From:  EasyRGB Team <support@easyrgb.com>
    To: Bryan Christ <bryan.christ@gmail.com>

    Subject: Re: EasyRGB.com contact = C library based on Easyrgb color
    conversion under a MIT license.


    Hi Bryan.

    No restrictions. Please reference the EasyRGB site in your doc and
    source files.  When you are done send us a link to your library, we can
    forward it to people asking for something similar.

    Best regards.

    Wang / Support

    On 19/01/11 02:13, Bryan Christ wrote:
    Are there any restrictions on the pseudo-code for color conversion on
    this site.  I would like to create a C library based on these under a MIT
    license.  Would that be okay?
*/

#include <math.h>

#include "macros.h"
#include "color_math.h"

// should be defined math.h but sometimes isn't
#ifndef M_PI
#define M_PI 3.1415926535
#endif

// #define F25POW7 6103515625.0F

#define DEG2RAD(angle)  ((angle) * (M_PI / 180.0))
#define RAD2DEG(angle)  ((angle) * (180.0 / M_PI))

float   cielab2hue(float a, float b);

/*
    R, G and, B (Standard RGB) input range = 0 - 255
*/
void
rgb2lab(int red, int green, int blue, float *l, float *a, float *b)
{
    float           rgb[] = { red, green, blue };
    float           xyz[] = { 0, 0, 0 };
    unsigned int    i;

    /*
        the first part of the computation is the same for the 3 RGB values.

        it converts the RGB values to CIEXYZ values.
    */
    for(i = 0; i < ARRAY_SZ(rgb); i++)
    {
        rgb[i] /= 255;

        if(rgb[i] > 0.04045)
        {
            rgb[i] = powf((rgb[i] + 0.055) / 1.055, 2.4);
        }
        else
        {
            rgb[i] /= 12.92;
        }

        rgb[i] *= 100;
    }

    // final conversion of RBG to CIEXYZ
    xyz[0] = rgb[0] * 0.412453 + rgb[1] * 0.357580 + rgb[2] * 0.180423;
    xyz[1] = rgb[0] * 0.212671 + rgb[1] * 0.715160 + rgb[2] * 0.072169;
    xyz[2] = rgb[0] * 0.019334 + rgb[1] * 0.119193 + rgb[2] * 0.950227;

    // XYZ Refernce Values (Daylight, sRGB, Adobe-RGB
    xyz[0] /= 95.047;
    xyz[1] /= 100.0;
    xyz[2] /= 108.883;

    // the second part of this function converts CIEXYZ to CIELAB 2000
    for(i = 0; i < ARRAY_SZ(xyz); i++)
    {
        if(xyz[i] > 0.008856)
        {
            xyz[i] = powf(xyz[i], (1.0 / 3.0));
        }
        else
        {
            xyz[i] = (xyz[i] * 7.787) + (16.0 / 116.0);
        }
    }

    *l = (xyz[1] * 116) - 16;
    *a = (xyz[0] - xyz[1]) * 500;
    *b = (xyz[1] - xyz[2]) * 200;

    return;
}

/*
    convert RGB to HSL system

    R, G and B input range = 0 - 255
    H, S and L output range = 0 - 1.0

    http://http://www.easyrgb.com/en/math.php
*/
void
rgb2hsl(float red, float green, float blue, float *h, float *s, float *l)
{
    float   min;
    float   max;
    float   delta_max;
    float   delta_red;
    float   delta_green;
    float   delta_blue;

    red /= 255;
    green /= 255;
    blue /= 255;

    // which color has the smallest value
    if((min = green < red ? green : red) > blue)
        min = blue;

    // which color has the largest value
    if((max = green > red ? green : red) < blue)
        max = blue;

    delta_max = max - min;

    // calculate lightness
    *l = (min + max) / 2.0;

    // this is shade of gray
    if(delta_max == 0)
    {
        *h = 0;
        *s = 0;
        return;
    }

    // calculate saturation
    if (*l < 0.5)
        *s = delta_max / (max + min);
    else
        *s = delta_max / (2.0 - max - min);

    // calculate hue
    delta_red = (((max - red) / 6.0) + (delta_max / 2.0)) / delta_max;
    delta_green = (((max - green) / 6.0) + (delta_max / 2.0)) / delta_max;
    delta_blue = (((max - blue) / 6.0) + (delta_max / 2.0)) / delta_max;

    if(max == red)
    {
        *h = delta_blue - delta_green;
    }

    if(max == green)
    {
        *h = (1.0 / 3.0) + delta_red - delta_blue;
    }

    if(max == blue)
    {
        *h = (2.0 / 3.0) + delta_green - delta_red;
    }

    if(*h < 0) *h += 1;
    if(*h > 1) *h -= 1;

    return;
}

/*
    oldest standard (1976).  less accurate.  faster.

    input:  color #1 LAB values
    input:  color #2 LAB values

    output: a float describing the percputual distance between 2 colors
*/
float
cie76_delta(float l1, float a1, float b1, float l2, float a2, float b2)
{
    float   delta_e;

    delta_e = sqrt(powf(l1 - l2, 2) + powf(a1 - a2, 2) + powf(b1 - b2, 2));

    return delta_e;
}

/*
    newest standard (2000).  most accurate.  computationally heavy.

    do not use:  incomplete code.
*/
/*
float
cie2k_delta(float l1, float a1, float b1, float l2, float a2, float b2)
{
    float   xc1, xc2;
    float   xcx;
    float   xcx_pow7;
    float   xgx;
    float   xnn;
    float   xh1, xh2;
    float   xdl;
    float   xdc;
    float   xdh;

    xc1 = sqrtf(a1 * a1 + b1 * b1);
    xc2 = sqrtf(a2 * a2 + b2 * b2);
    xcx = (xc1 + xc2) / 2.0;

    xcx_pow7 = powf(xcx, 7);
    xgx = 0.5 * (1.0 - sqrtf(xcx_pow7 / (xcx_pow7 + F25POW7))); 

    xnn = (1 + xgx) * a1;
    xc1 = sqrtf(xnn * xnn + b1 * b1);
    xh1 = cielab2hue(xnn, b1);

    xnn = (1 + xgx) * a2;
    xc2 = sqrtf(xnn * xnn + b2 * b2);
    xh2 = cielab2hue(xnn, b2);

    xdl = l2 - l1;
    xdc = xc2 - xc1;

    if((xc1 * xc2) == 0) xdh = 0;
    else
    {
        xnn = roundf(xh2 - xh1);

        if(fabsf(xnn) <= 180.0) xdh = xh2 - xh1;
        else
        {
            if(xnn > 180.0) xdh = xh2 - xh1 - 360.0;
            else xdh = xh2 - xh1 + 360.0;
        }
    }

    // todo

    return 0.0;
}
*/

float
cielab2hue(float a, float b)
{
    float   bias = 0;

    if(a >= 0 && b == 0) return 0;
    if(a < 0 && b == 0) return 180;
    if(a == 0 && b > 0) return 90;
    if(a == 0 && b < 0) return 270;
    if(a > 0 && b > 0) bias = 0;
    if(a < 0 ) bias = 180;
    if(a > 0 && b < 0) bias = 360;

    return RAD2DEG(atanf(b / a)) + bias;
}

 
