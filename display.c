#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "display_defines.h"
#include "display.h"


// placeholder for actual brightness control see https://forum.pine64.org/showthread.php?tid=9378, pwm is planned
void display_backlight(char brightness) {
    nrf_gpio_cfg_output(LCD_BACKLIGHT_HIGH);	
    if (brightness != 0) {
        nrf_gpio_pin_write(LCD_BACKLIGHT_HIGH,0);
    } else {
        nrf_gpio_pin_write(LCD_BACKLIGHT_HIGH,1);
    }
}

// send one byte over spi
void display_send(bool mode, uint8_t byte) {
    nrf_gpio_pin_write(LCD_COMMAND,mode);


    NRF_SPIM0->TXD.MAXCNT = 1;
    NRF_SPIM0->TXD.PTR = (uint32_t)&byte;

    NRF_SPIM0->EVENTS_ENDTX = 0;
    NRF_SPIM0->EVENTS_ENDRX = 0;
    NRF_SPIM0->EVENTS_END = 0;

    NRF_SPIM0->TASKS_START = 1;
    while(NRF_SPIM0->EVENTS_ENDTX == 0) {__NOP();};
    while(NRF_SPIM0->EVENTS_END == 0){__NOP();};
    NRF_SPIM0->TASKS_STOP = 1;
    while (NRF_SPIM0->EVENTS_STOPPED == 0){__NOP();};
}

// send a bunch of bytes from buffer
void display_sendbuffer(bool mode, uint8_t* m_tx_buf, int m_length) {
    NRF_SPIM0->TXD.MAXCNT = m_length;
    NRF_SPIM0->TXD.PTR = (uint32_t)m_tx_buf;

    NRF_SPIM0->EVENTS_ENDTX = 0;
    NRF_SPIM0->EVENTS_ENDRX = 0;
    NRF_SPIM0->EVENTS_END = 0;

    NRF_SPIM0->TASKS_START = 1;
    while(NRF_SPIM0->EVENTS_ENDTX == 0);
    while(NRF_SPIM0->EVENTS_END == 0);
    NRF_SPIM0->TASKS_STOP = 1;
    while (NRF_SPIM0->EVENTS_STOPPED == 0);
}

// send a bunch of bytes from buffer
void display_sendbuffer_noblock(uint8_t* m_tx_buf, int m_length) {
    NRF_SPIM0->TXD.MAXCNT = m_length;
    NRF_SPIM0->TXD.PTR = (uint32_t)&m_tx_buf[0];

    NRF_SPIM0->EVENTS_ENDTX = 0;
    NRF_SPIM0->EVENTS_ENDRX = 0;
    NRF_SPIM0->EVENTS_END = 0;

    NRF_SPIM0->TASKS_START = 1;
}

// this function must be called after display_sendbuffer_noblock has been called
// and before the next call of spim related functions. It will wait for spim to
// finish and will then stop spim0
void display_sendbuffer_finish() {
    while(NRF_SPIM0->EVENTS_ENDTX == 0);
    while(NRF_SPIM0->EVENTS_END == 0);
    NRF_SPIM0->TASKS_STOP = 1;
    while (NRF_SPIM0->EVENTS_STOPPED == 0);
}

#define ppi_set() NRF_PPI->CHENSET = 0xff; // enable first 8 ppi channels
#define ppi_clr() NRF_PPI->CHENCLR = 0xff; // disable first 8 ppi channels

void display_init() {
    ////////////////
    // setup pins //
    ////////////////
    nrf_gpio_cfg_output(LCD_MOSI);
    nrf_gpio_cfg_output(LCD_SCK);
    nrf_gpio_cfg_input(LCD_MISO, NRF_GPIO_PIN_NOPULL);

    nrf_gpio_cfg_output(LCD_SELECT);
    nrf_gpio_cfg_output(LCD_COMMAND);
    nrf_gpio_cfg_output(LCD_RESET);

    nrf_gpio_pin_write(LCD_SELECT,1);
    nrf_gpio_pin_write(LCD_COMMAND,1);
    nrf_gpio_pin_write(LCD_RESET,1);


    ///////////////
    // spi setup //
    ///////////////

   // nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
   // spi_config.frequency      = NRF_SPIM_FREQ_8M;
   // spi_config.ss_pin         = NRFX_SPIM_PIN_NOT_USED;
   // spi_config.miso_pin       = LCD_MISO;
   // spi_config.mosi_pin       = LCD_MOSI;
   // spi_config.sck_pin        = LCD_SCK;
   // spi_config.mode           = NRF_SPIM_MODE_3;
   // nrfx_spim_init(&spi, &spi_config, spim_event_handler, NULL);
    NRF_SPIM0->PSEL.SCK  = LCD_SCK;
    NRF_SPIM0->PSEL.MOSI = LCD_MOSI;
    NRF_SPIM0->PSEL.MISO = LCD_MISO;

    uint32_t config = (SPIM_CONFIG_ORDER_MsbFirst);

    config |= (SPIM_CONFIG_CPOL_ActiveLow  << SPIM_CONFIG_CPOL_Pos) |
              (SPIM_CONFIG_CPHA_Trailing   << SPIM_CONFIG_CPHA_Pos);

    NRF_SPIM0->CONFIG = config;
    NRF_SPIM0->FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M8 << SPIM_FREQUENCY_FREQUENCY_Pos;
    NRF_SPIM0->ENABLE = SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos;




    ///////////////////
    // reset display //
    ///////////////////
    nrf_gpio_pin_write(LCD_RESET,0);
    nrf_delay_ms(200);
    nrf_gpio_pin_write(LCD_RESET,1);
    nrf_delay_ms(200);
    nrf_gpio_pin_write(LCD_SELECT,0);


    display_send (0, CMD_SWRESET);
    display_send (0, CMD_SLPOUT);

    display_send (0, CMD_COLMOD);
    display_send (1, 0x55);

    display_send (0, CMD_MADCTL); 
    display_send (1, 0x00);

    display_send (0, CMD_INVON); // for standard 16 bit colors
    display_send (0, CMD_NORON);
    display_send (0, CMD_DISPON);



    ///////////////////////////
    // setup LCD_COMMAND PIN //
    ///////////////////////////
    nrf_gpio_cfg_output(LCD_COMMAND);


    NRF_TIMER3->MODE = 0 << TIMER_MODE_MODE_Pos; // timer mode
    NRF_TIMER3->BITMODE = 0 << TIMER_BITMODE_BITMODE_Pos; // 16 bit
    NRF_TIMER3->PRESCALER = 0 << TIMER_PRESCALER_PRESCALER_Pos; // 16 MHz

    // the following CC setup will cause byte 0, 5 and 10 
    // of any SPIM0 dma transfer to be treated as CMD bytes
    NRF_TIMER3->CC[0] = 5+(0*2);
    NRF_TIMER3->CC[1] = 5+(8*2);
    NRF_TIMER3->CC[2] = 5+(40*2);
    NRF_TIMER3->CC[3] = 5+(48*2);
    NRF_TIMER3->CC[4] = 5+(80*2);
    NRF_TIMER3->CC[5] = 5+(88*2);


    // create GPIOTE task to switch LCD_COMMAND pin
    NRF_GPIOTE->CONFIG[1] = GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos |
        GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos |
        LCD_COMMAND << GPIOTE_CONFIG_PSEL_Pos | 
        GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos;


    // PPI channels for toggeling pin
    for (int channel = 0; channel < 6; channel++) { 
        NRF_PPI->CH[channel].EEP = (uint32_t) &NRF_TIMER3->EVENTS_COMPARE[channel];
        if (channel % 2)
            NRF_PPI->CH[channel].TEP = (uint32_t) &NRF_GPIOTE->TASKS_SET[1];
        else 
            NRF_PPI->CH[channel].TEP = (uint32_t) &NRF_GPIOTE->TASKS_CLR[1];
    }


    NRF_PPI->CH[6].EEP = (uint32_t) &NRF_SPIM0->EVENTS_STARTED;
    NRF_PPI->CH[6].TEP = (uint32_t) &NRF_TIMER3->TASKS_CLEAR;

    NRF_PPI->CH[7].EEP = (uint32_t) &NRF_SPIM0->EVENTS_STARTED;
    NRF_PPI->CH[7].TEP = (uint32_t) &NRF_TIMER3->TASKS_START;

    //  NRF_PPI->CH[8].EEP = (uint32_t) &NRF_TIMER3->EVENTS_COMPARE[5];
    //  NRF_PPI->CH[8].TEP = (uint32_t) &NRF_TIMER3->TASKS_STOP;


}


void drawSquare(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    ppi_set();

    int maxLength = 254; // TODO: this should be TXD.MAXCNT
    uint8_t byteArray [maxLength + 1];

    // addresses are offset by 1 to give the ability to recycle the array
    /* setup display for writing */
    byteArray[1] = CMD_CASET;

    byteArray[2] = x1 >> 8;
    byteArray[3] = x1 & 0xff;

    byteArray[4] = x2 >> 8;
    byteArray[5] = x2 & 0xff;

    byteArray[6] = CMD_RASET;

    byteArray[7] = y1 >> 8;
    byteArray[8] = y1 & 0xff;

    byteArray[9] = y2 >> 8;
    byteArray[10] = y2 & 0xff;

    byteArray[11] = CMD_RAMWR;
    /**/

    int area = (x2-x1+1)*(y2-y1+1);

    int areaToWrite;
    if (area > (maxLength - 11) / 2)
        areaToWrite = (maxLength - 11) / 2;
    else 
        areaToWrite = area;


    for (int i = 0; i < areaToWrite; i++) {
        byteArray[12+i*2] = color >> 8;
        byteArray[12+i*2+1] = color & 0xff;
    }

    area -= areaToWrite;

    // non blocking SPI here is negligible and unreliable cause stuff 
    // would be written memory while sending that same memory
    display_sendbuffer(0, byteArray + 1, (areaToWrite * 2)+11);
    ppi_clr();

    if (area > 0) {
        for (int i = 0; i < 6; i++) {
            byteArray[i*2] = color >> 8;
            byteArray[i*2+1] = color;
        }

        while (area > 0) {
            if (area > maxLength / 2)
                areaToWrite = maxLength / 2;
            else 
                areaToWrite = area;

            area -= areaToWrite;

            display_sendbuffer(0, byteArray, areaToWrite * 2);
        }
    }
}


void drawBitmap (uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t* bitmap) {
    ppi_set();

    int maxLength = 254; // TODO: this should be TXD.MAXCNT
    uint8_t byteArray[maxLength];

    /* setup display for writing */
    byteArray[0] = CMD_CASET;

    byteArray[1] = x1 >> 8;
    byteArray[2] = x1 & 0xff;

    byteArray[3] = x2 >> 8;
    byteArray[4] = x2 & 0xff;

    byteArray[5] = CMD_RASET;

    byteArray[6] = y1 >> 8;
    byteArray[7] = y1 & 0xff;

    byteArray[8] = y2 >> 8;
    byteArray[9] = y2 & 0xff;

    byteArray[10] = CMD_RAMWR;
    /**/


    int areaToWrite;
    int area = (x2-x1+1)*(y2-y1+1);

    if (area > maxLength / 2 - 11)
        areaToWrite = maxLength / 2 - 11;
    else 
        areaToWrite = area;


    for (int i = 0; i < areaToWrite; i++) {
        byteArray[i*2 + 11] = bitmap[i*2];
        byteArray[i*2+1 + 11] = bitmap[i*2+1];
    }

    area -= areaToWrite;

    display_sendbuffer(0, byteArray, (areaToWrite * 2)+11);

    ppi_clr();

    int offset = 0;

    while (area > 0) {
        offset += areaToWrite*2;

        if (area > maxLength / 2)
            areaToWrite = maxLength / 2;
        else 
            areaToWrite = area;

        display_sendbuffer(0, bitmap+offset, areaToWrite * 2);
        ppi_clr();
        area -= areaToWrite;
    }
}


void drawMono(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t* frame, uint16_t posColor, uint16_t negColor) {
    ppi_set();

    int maxLength = 254; // TODO: this should be TXD.MAXCNT
    uint8_t byteArray0[maxLength];
    uint8_t byteArray1[maxLength];


    /* setup display for writing */
    byteArray0[0] = CMD_CASET;

    byteArray0[1] = x1 >> 8;
    byteArray0[2] = x1 & 0xff;

    byteArray0[3] = x2 >> 8;
    byteArray0[4] = x2 & 0xff;

    byteArray0[5] = CMD_RASET;

    byteArray0[6] = y1 >> 8;
    byteArray0[7] = y1 & 0xff;

    byteArray0[8] = y2 >> 8;
    byteArray0[9] = y2 & 0xff;

    byteArray0[10] = CMD_RAMWR;
    /**/


    int area = (x2-x1+1)*(y2-y1+1);


    int pixel = 0;
    int byte = 11;
    int bytesToSend = byte + area*2;
    int packet = 0;

    while (area > pixel) {
        uint8_t* byteArray;
        if (packet % 2 == 0) 
            byteArray = byteArray0;
        else 
            byteArray = byteArray1;


        while (byte < maxLength - 1 && byte < bytesToSend) {
            uint16_t color = 0;

            if ((frame[pixel / 8] >> (pixel % 8)) & 1) {
                color = posColor;
            } else {
                color = negColor;
            }
            byteArray[byte] = color >> 8;
            byte++;
            byteArray[byte] = color;
            byte++;

            pixel++;
        }



        if (packet > 0) {
            display_sendbuffer_finish();
            ppi_clr();
        }
        display_sendbuffer_noblock(byteArray, byte);

        bytesToSend -= byte;

        byte = 0;

        packet++;
    }
    display_sendbuffer_finish();
}

void scroll(uint16_t TFA, uint16_t VSA, uint16_t BFA, uint16_t scroll_value) {
    /* set square to draw in */
    display_send (0, CMD_VSCRDEF);

    display_send (1,TFA >> 8);
    display_send (1,TFA & 0xff);

    display_send (1,VSA >> 8);
    display_send (1,VSA & 0xff);

    display_send (1,BFA >> 8);
    display_send (1,BFA & 0xff);

    display_send (0, CMD_MADCTL);
    display_send (1, 0x0/*0x10*/);


    display_send (0,CMD_VSCSAD);

    display_send (1,scroll_value >> 8);
    display_send (1,scroll_value & 0xff);

    /**/
}

void partialMode(uint16_t PSL, uint16_t PEL) {
    display_send (0, CMD_MADCTL);
    display_send (1, 0x0/*0x10*/);

    display_send (0, CMD_PTLAR);

    display_send (1,PSL >> 8);
    display_send (1,PSL & 0xff);

    display_send (1,PEL >> 8);
    display_send (1,PEL & 0xff);

    display_send (0, CMD_PTLON);

}
