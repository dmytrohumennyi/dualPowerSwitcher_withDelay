#include <xc.h>
#include <stdint.h>

#pragma config FOSC = INTRCIO
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config MCLRE = ON
#pragma config BOREN = ON
#pragma config CP = OFF
#pragma config CPD = OFF

#define _XTAL_FREQ 4000000UL
#define BIT(b) (1u << (b))

enum { LED1=0, LED2=1, CH1=2, CH2=4, SNS=5 };

static uint8_t out_shadow;
static const uint8_t OUT_MASK = BIT(LED1)|BIT(LED2)|BIT(CH1)|BIT(CH2);

static inline void apply_out(void)
{
    uint8_t g = GPIO;
    g &= (uint8_t)~OUT_MASK;
    g |= (out_shadow & OUT_MASK);
    GPIO = g;
}

static inline void set_bit(uint8_t b, uint8_t v)
{
    if (v) out_shadow |= BIT(b);
    else   out_shadow &= (uint8_t)~BIT(b);
    apply_out();
}

// inv outputs: 0=ON, 1=OFF
static inline void all_off(void)
{
    out_shadow |= BIT(CH1);
    out_shadow |= BIT(CH2);
    apply_out();
}
static inline void city_on(void)   { out_shadow &= (uint8_t)~BIT(CH2); out_shadow |= BIT(CH1); apply_out(); }
static inline void batt_on(void)   { out_shadow &= (uint8_t)~BIT(CH1); out_shadow |= BIT(CH2); apply_out(); }

static inline uint8_t city_present(void)
{
    // sensor inv: 0 => city present
    return (GPIO & BIT(SNS)) == 0;
}

int main(void)
{
    ANSEL = 0x00;
    CMCON = 0x07;

    TRISIO = 0x00;
    TRISIO |= BIT(SNS); // GP5 input

    // start safe: LEDs off, both channels OFF
    out_shadow = 0;
    out_shadow |= BIT(CH1) | BIT(CH2);
    apply_out();

    // mandatory startup interlock
    __delay_ms(2000);

    // initial select
    if (city_present()) city_on();
    else                batt_on();

    uint16_t t1=0, t2=0;
    uint8_t hb=0, l2=0;
    uint8_t src_city = city_present(); // 1=city, 0=batt

    while (1)
    {
        __delay_ms(50);
        t1 += 50;

        // LED1 heartbeat
        if (t1 >= 1000) {
            t1 = 0;
            hb ^= 1u;
            set_bit(LED1, hb);
        }

        // source decision (no filter)
        {
            uint8_t want_city = city_present();
            if (want_city != src_city)
            {
                // interlock 2s every switch
                all_off();
                __delay_ms(2000);

                if (want_city) city_on();
                else           batt_on();

                src_city = want_city;
                t2 = 0; l2 = 0;
            }
        }

        // LED2 according to current source
        if (src_city)
        {
            set_bit(LED2, 1);
            t2 = 0; l2 = 0;
        }
        else
        {
            t2 += 50;
            if (t2 >= 2000) {
                t2 = 0;
                l2 ^= 1u;
                set_bit(LED2, l2);
            }
        }
    }
}