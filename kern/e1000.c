#include <kern/e1000.h>
#include <inc/error.h>

// LAB 6: Your driver code here
uint32_t mac[2] = {0x12005452, 0x5634};
volatile uint32_t *e1000_mem_regs;

struct tx_desc tx_descs[TX_DESC_NUM];
struct e1000_packet_buf e1000_tx_packet_bufs[TX_DESC_NUM];
struct rx_desc rx_descs[RX_DESC_NUM];
struct e1000_packet_buf e1000_rx_packet_bufs[RX_DESC_NUM];

//char test_packet[] = "Hello, world!";

int try_tx_packet(const void *src, size_t n);
int tx_descs_init(void);
int try_rx_packet(void *dst, int *n);
int rx_descs_init(void);

int pci_e1000_init(struct pci_func *f){
  pci_func_enable(f);
  e1000_mem_regs = mmio_map_region(f->reg_base[0], f->reg_size[0]);
  // transmit initialization
  e1000_mem_regs[E1000_TDBAL/4] = PADDR(tx_descs);
  e1000_mem_regs[E1000_TDLEN/4] = sizeof(tx_descs);
  e1000_mem_regs[E1000_TDH/4] = 0;
  e1000_mem_regs[E1000_TDT/4] = 0;
  e1000_mem_regs[E1000_TCTL/4] = E1000_TCTL_EN | E1000_TCTL_PSP |
    (E1000_TCTL_CT & (0x10 << 4)) | (E1000_TCTL_COLD & (0x40 << 12));
  e1000_mem_regs[E1000_TIPG/4] = 10 | (4 << 10) | (6 << 20);
  tx_descs_init();
  //try_tx_packet(test_packet, sizeof(test_packet));
  //receive initialization
  e1000_mem_regs[E1000_RDBAL/4] = PADDR(rx_descs);
  e1000_mem_regs[E1000_RDLEN/4] = sizeof(rx_descs);
  // multicast and interruption retlated initialization is skipped
  e1000_mem_regs[E1000_RDH/4] = 0;
  e1000_mem_regs[E1000_RDT/4] = RX_DESC_NUM - 1;
  e1000_mem_regs[E1000_RCTL/4] = E1000_RCTL_EN | E1000_RCTL_SECRC | E1000_RCTL_BSIZE;
  rx_descs_init();
  e1000_mem_regs[E1000_RAL(0)] = mac[0];
  e1000_mem_regs[E1000_RAH(0)] = mac[1];
  e1000_mem_regs[E1000_RAH(0)] |= E1000_RAH_AV;

  // leave (RCTL.EN = 0b) until after the receive descriptor ring has been
  // initialized and software is ready to process received packets.
  //e1000_mem_regs[E1000_RCTL/4] |= E1000_RCTL_EN;
  return 0;
}

int try_tx_packet(const void *src, size_t n)
{
  struct tx_desc *desc;
  int tail = e1000_mem_regs[E1000_TDT/4];
  if(n >= MAX_PACKET_SIZE)
    n = MAX_PACKET_SIZE;
  desc = &tx_descs[tail];
  if(!(desc->status & E1000_TXD_STAT_DD)){
    return -1;
  }
  desc->status = ~E1000_TXD_STAT_DD;
  desc->length = n;
  desc->cmd |= E1000_TXD_CMD_EOP >> 24;
  memcpy(&e1000_tx_packet_bufs[tail], src, n);
  e1000_mem_regs[E1000_TDT/4] = (tail + 1) % TX_DESC_NUM;
  return 0;
}

int try_rx_packet(void *dst, int *n){
  struct rx_desc *desc;
  int tail = (e1000_mem_regs[E1000_RDT/4] + 1) % RX_DESC_NUM;
  desc = &rx_descs[tail];
  if(!(desc->status & E1000_RXD_STAT_DD)){
    return -1;
  }
  *n = desc->length;
  memcpy(dst, &e1000_rx_packet_bufs[tail], desc->length);
  desc->status = 0;
  e1000_mem_regs[E1000_RDT/4] = tail;
  return 0;
}

int tx_descs_init(void){
  for(int i = 0; i < TX_DESC_NUM; i++){
    tx_descs[i].addr = PADDR(&e1000_tx_packet_bufs[i]);
    tx_descs[i].cmd |= (E1000_TXD_CMD_RPS | E1000_TXD_CMD_IDE | E1000_TXD_CMD_RS) >> 24;
    tx_descs[i].status |= E1000_TXD_STAT_DD;
  }
  return 0;
}

int rx_descs_init(void){
  for(int i = 0; i < RX_DESC_NUM; i++){
    memset(&rx_descs[i], 0, sizeof(struct rx_desc));
    rx_descs[i].addr = PADDR(&e1000_rx_packet_bufs[i]);
  }
  return 0;
}
