#include <kern/e1000.h>
#include <inc/error.h>

// LAB 6: Your driver code here
volatile uint32_t *e1000_mem_regs;

struct tx_desc tx_descs[TX_DESC_NUM];
struct e1000_packet_buf e1000_packet_bufs[TX_DESC_NUM];

char test_packet[] = "Hello, world!";

int try_tx_packet(const void *src, size_t n);
int tx_descs_init(void);

int pci_e1000_init(struct pci_func *f){
  pci_func_enable(f);
  e1000_mem_regs = mmio_map_region(f->reg_base[0], f->reg_size[0]);
  // cprintf("device status reg: 0x%x\n", *(uint32_t *)((char *)e1000_mem_regs + 8));
  e1000_mem_regs[E1000_TDBAL/4] = PADDR(tx_descs);
  e1000_mem_regs[E1000_TDLEN/4] = sizeof(tx_descs);
  e1000_mem_regs[E1000_TDH/4] = 0;
  e1000_mem_regs[E1000_TDT/4] = 0;
  e1000_mem_regs[E1000_TCTL/4] = E1000_TCTL_EN | E1000_TCTL_PSP |
    (E1000_TCTL_CT & (0x10 << 4)) | (E1000_TCTL_COLD & (0x40 << 12));
  e1000_mem_regs[E1000_TIPG/4] = 10 | (4 << 10) | (6 << 20);
  tx_descs_init();
  //try_tx_packet(test_packet, sizeof(test_packet));
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
  desc->status &= ~E1000_TXD_STAT_DD;
  desc->length = n;
  desc->cmd |= E1000_TXD_CMD_EOP >> 24;
  memcpy(&e1000_packet_bufs[tail], src, n);
  e1000_mem_regs[E1000_TDT/4] = (tail + 1) % TX_DESC_NUM;
  return 0;
}

int tx_descs_init(void){
  for(int i = 0; i < TX_DESC_NUM; i++){
    tx_descs[i].addr = PADDR(&e1000_packet_bufs[i]);
    tx_descs[i].cmd |= (E1000_TXD_CMD_RPS | E1000_TXD_CMD_IDE | E1000_TXD_CMD_RS) >> 24;
    tx_descs[i].status |= E1000_TXD_STAT_DD;
  }
  return 0;
}
