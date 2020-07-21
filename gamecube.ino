#include <OneWire.h>

#ifndef __MKL26Z64__
#error "Womp :(. This only works with the Teensy LC"
#endif

// Most of this was ripped off from 
//   github.com:PaulStoffregen/OneWire OneWire.cpp

#define DIRECT_READ(base, mask)         ((*((base)+16) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   (*((base)+20) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask)  (*((base)+20) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)    (*((base)+8) = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   (*((base)+4) = (mask))

enum { 
    LED_PIN  = 13,
    DATA_PIN = 16,
};

struct State {
    uint8_t a;
    uint8_t b;
    uint8_t x;
    uint8_t y;
    uint8_t l;
    uint8_t r;
    uint8_t z;
    uint8_t start;
    uint8_t d_up;
    uint8_t d_down;
    uint8_t d_left;
    uint8_t d_right;
    uint8_t joy_x;
    uint8_t joy_y;
    uint8_t c_x;
    uint8_t c_y;
    uint8_t l_var;
    uint8_t r_var;
};

static uint8_t  data_pin_mask;
static volatile uint8_t *data_pin_base;

void
setup()
{        
    pinMode(DATA_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    
    data_pin_base = portOutputRegister(DATA_PIN);
    data_pin_mask = digitalPinToBitMask(DATA_PIN);

    digitalWrite(LED_PIN, 0);
}

void
write_one()
{
    uint8_t mask = data_pin_mask;
    volatile uint8_t *reg = data_pin_base;

    DIRECT_MODE_OUTPUT(reg, mask);
    DIRECT_WRITE_LOW(reg, mask);
    delayMicroseconds(1);
    DIRECT_MODE_INPUT(reg, mask);
    delayMicroseconds(3);
}

void
write_zero()
{
    uint8_t mask = data_pin_mask;
    volatile uint8_t *reg = data_pin_base;
  
    DIRECT_MODE_OUTPUT(reg, mask);
    DIRECT_WRITE_LOW(reg, mask);
    // TODO: try 2 instead to account for pull up time.
    delayMicroseconds(3);
    DIRECT_MODE_INPUT(reg, mask);
    delayMicroseconds(1);
}

void
write_bit(uint8_t bit)
{
    if (bit) {
        write_one();
    } else {
        write_zero();
    }
}

void
write_byte(uint8_t v)
{
    uint8_t mask;
  
    for (mask = 0x80; mask; mask >>= 1) {
        write_bit((mask & v) ? 1 : 0);
    }
}

FASTRUN
void poll()
{
    // The magic contoller polling sequence
    write_byte(0x40);
    write_byte(0x03);
    write_byte(0x02);
    write_bit(1);       // Stop bit
}

void
unmarshal(State *s, uint64_t v)
{
    s->a       = !!(v & 0x0100000000000000);
    s->b       = !!(v & 0x0200000000000000);
    s->x       = !!(v & 0x0400000000000000);
    s->y       = !!(v & 0x0800000000000000);
    s->l       = !!(v & 0x0040000000000000);
    s->r       = !!(v & 0x0020000000000000);
    s->z       = !!(v & 0x0010000000000000);
    s->d_up    = !!(v & 0x0008000000000000);
    s->d_down  = !!(v & 0x0004000000000000);
    s->d_right = !!(v & 0x0002000000000000);
    s->d_left  = !!(v & 0x0001000000000000);
    s->start   = !!(v & 0x1000000000000000);
    s->joy_x   = ((v >> 40) & 0xFF); 
    s->joy_y   = ((v >> 32) & 0xFF);
    s->c_x     = ((v >> 24) & 0xFF); 
    s->c_y     = ((v >> 16) & 0xFF); 
    s->l_var   = ((v >> 13) & 0x07);
    s->r_var   = ((v >>  5) & 0x07); 
}

State last;

FASTRUN
void loop()
{
    noInterrupts();
    delayMicroseconds(1000);
    
    poll();
   
    uint64_t v = 0;
    for (int i = 0; i < 64; i++) {
        uint8_t r;
        while (DIRECT_READ(data_pin_base, data_pin_mask))
            ;
        delayMicroseconds(2);
        r = DIRECT_READ(data_pin_base, data_pin_mask);
        while (!DIRECT_READ(data_pin_base, data_pin_mask))
            ;
        //delayMicroseconds(1);
        v = (v << 1) | r;
    }

    interrupts();

    State curr;
    unmarshal(&curr, v);

    if (curr.a != last.a)
        Joystick.button(1, curr.a);
    if (curr.b != last.b)
        Joystick.button(2, curr.b);
    if (curr.x != last.x)
        Joystick.button(3, curr.x);
    if (curr.y != last.y)
        Joystick.button(4, curr.y);
    if (curr.l != last.l)
        Joystick.button(5, curr.l);
    if (curr.r != last.r)
        Joystick.button(6, curr.r);
    if (curr.z != last.z)
        Joystick.button(7, curr.z);
    if (curr.start != last.start)
        Joystick.button(8, curr.start);
    if (curr.d_up != last.d_up)
        Joystick.button(9, curr.d_up);
    if (curr.d_down != last.d_down)
        Joystick.button(10, curr.d_down);
    if (curr.d_right != last.d_right)
        Joystick.button(11, curr.d_right);
    if (curr.d_left != last.d_left)
        Joystick.button(12, curr.d_left);
        
    Joystick.X(curr.joy_x * 4);
    Joystick.Y(curr.joy_y * 4);
    Joystick.Z(curr.c_x * 4);
    Joystick.Zrotate(curr.c_y * 4);
    Joystick.sliderRight(512 + (curr.r_var * 32));

    memcpy(&last, &curr, sizeof(State));
}
