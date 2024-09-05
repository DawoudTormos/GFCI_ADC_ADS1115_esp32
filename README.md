
## GFCI 
A GFCI Ground Fault Circuit Interrupter Device.
It checks for  ground fault where electricity escapes to ground out of the main 2 lines which happens in most cases of a human being electrically Shot.
So when this is detected, we can at that point cut off the main breaker to stop the person being electrocuted from dying.

Such a device is made through electronic (circuit-level), mechanical and electrical ways. Actually, the way a GFCI is made is magical and so good.
 However, as always, a mechanical is bound to fail at some point and could be not the fastest. In a GFCI breaker or differential breaker (same thing), they keep for the user a button that imitate what happens in ground-fault to test if the breaker is still good.

## Goal
I am doing the same thing but based on an MCU (esp32 in my case) and using an accurate precise(16-bit with 15-bit actual accuracy) ADC  (ADS1115) and a CT (current transformer).
I aim at being faster (or equally fast at least) and for the device to live longer.
Another plus my device offer is that it automatically turns ON the main lines after sometime.
 Also I can later make it an IOT device with functionalities an esp32 offers.
 

