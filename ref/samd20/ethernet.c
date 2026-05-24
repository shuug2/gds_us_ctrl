
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <asf.h>
//#include "pico/binary_info.h"
//#include "hardware/spi.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include "ethernet.h"

wiz_NetInfo gWIZNETINFO = { .mac = {0x00,0x08,0xdc,0x78,0x91,0x71},
                            .ip = {192,168,0,15},
                            .sn = {255,255,255,255},
                            .gw = {192,168,0,1},
                            .dns = {168,126,63,1},
                            .dhcp = NETINFO_STATIC};

unsigned char ethBuf0[ETH_MAX_BUF];
struct spi_module eth_spi_instance;

void print_network_information(void);


static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    ETH_SS_LOW();
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    ETH_SS_HIGH();
    asm volatile("nop \n nop \n nop");
}

static void wiznet_reset() {
    // Two byte reset. First byte register, second byte data
    // There are a load more options to set up the device in different ways that could be added here

    ETH_NRST_HIGH();
    ETH_NRST_LOW();
    delay_ms(100);
    ETH_NRST_HIGH();
    delay_ms(200);
}

uint8_t  wiznet_read(void) {
    // For this particular device, we send the device the register we want to read
    // first, then subsequently read from the device. The register is auto incrementing
    // so we don't need to keep sending the register we want, just the first.
    uint16_t rx = 0, tx = 0xFF;
    spi_transceive_wait(&eth_spi_instance, tx,  &rx);
    return rx;
        
}

static void wiznet_write(uint8_t reg) {
 
	uint16_t rx = 0;
    spi_transceive_wait(&eth_spi_instance, (uint16_t)reg, &rx);
}

void print_network_information(void) 
{
    uint8_t tmpstr[6] = {0,};

	wizchip_getnetinfo(&gWIZNETINFO);
	/*
    if(gWIZNETINFO.dhcp == NETINFO_DHCP) 
        printf("\r\n===== %s NET CONF : DHCP =====\r\n",(char*)tmpstr);
	else 
        printf("\r\n===== %s NET CONF : Static =====\r\n",(char*)tmpstr);

	printf("Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n\r",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
	printf("IP address : %d.%d.%d.%d\n\r",gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
	printf("SM Mask	   : %d.%d.%d.%d\n\r",gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
	printf("Gate way   : %d.%d.%d.%d\n\r",gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
	printf("DNS Server : %d.%d.%d.%d\n\r",gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
	*/
}

void Net_Conf(void)
{
	/* wizchip netconf */
	ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO);
}

void configure_spi_master(void)
{
	struct spi_config config_spi_master;
	/* Configure, initialize and enable SERCOM SPI module */
	spi_get_config_defaults(&config_spi_master);
	config_spi_master.mux_setting = ETH_SERCOM_MUX_SETTING;
	config_spi_master.pinmux_pad0 = ETH_SERCOM_PINMUX_PAD0;
	config_spi_master.pinmux_pad1 = ETH_SERCOM_PINMUX_PAD1;
	config_spi_master.pinmux_pad2 = ETH_SERCOM_PINMUX_PAD2;
	config_spi_master.pinmux_pad3 = ETH_SERCOM_PINMUX_PAD3;
	config_spi_master.generator_source = GCLK_GENERATOR_3;
	config_spi_master.mode_specific.master.baudrate = 8000000;	// 4MHz
	spi_init(&eth_spi_instance, ETH_MODULE, &config_spi_master);
	spi_enable(&eth_spi_instance);
}
    
int ethernet_init(void)
{
    int8_t phy_link =0;
    uint8_t id=0, error_cnt;

    //->printf("Initialize W5500...\n");

    // This example will use SPI0 at 0.5MHz.
    //spi_init(SPI_PORT, 5000 * 1000);
    //gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    //gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    //gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    //bi_decl(bi_3pins_with_func(PIN_MISO, PIN_MOSI, PIN_SCK, GPIO_FUNC_SPI));
	configure_spi_master();

    // Chip select is active-low, so we'll initialise it to a driven-high state
    //gpio_init(PIN_CS);
    //gpio_set_dir(PIN_CS, GPIO_OUT);
    //gpio_put(PIN_CS, 1);
    // Make the CS pin available to picotool
    //bi_decl(bi_1pin_with_name(PIN_CS, "SPI CS"));
	ETH_SS_HIGH();

    //gpio_init(PIN_RST);
    //gpio_set_dir(PIN_RST, GPIO_OUT);
    //gpio_put(PIN_RST, 0);
    // Make the CS pin available to picotool
    //bi_decl(bi_1pin_with_name(PIN_RST, "SPI RST"));
	ETH_NRST_HIGH();


    wiznet_reset();

    reg_wizchip_spi_cbfunc(wiznet_read,wiznet_write);
    reg_wizchip_cs_cbfunc(cs_select,cs_deselect);

    id =getVERSIONR();
    //->printf("Version is 0x%x\n", id);

    error_cnt = 0;
    do{//check phy status
        if(ctlwizchip(CW_GET_PHYLINK, &phy_link)== -1){
            delay_ms(10);
        }
        if(++error_cnt > 99)
            return -1;
    }while(phy_link == PHY_LINK_OFF);
    ctlnetwork(CN_SET_NETINFO, &gWIZNETINFO);
    return 0;
}    
