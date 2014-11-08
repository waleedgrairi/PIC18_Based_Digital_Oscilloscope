/************************ Devices connection part *************************/
// GLCD module connections
char GLCD_DataPort at PORTD;
sbit GLCD_CS1 at Rb0_bit;
sbit GLCD_CS2 at Rb1_bit;
sbit GLCD_RS  at Rb2_bit;
sbit GLCD_RW  at Rb3_bit;
sbit GLCD_EN  at Rb4_bit;
sbit GLCD_RST at Rb5_bit;

sbit GLCD_CS1_Direction at TRISb0_bit;
sbit GLCD_CS2_Direction at TRISb1_bit;
sbit GLCD_RS_Direction  at TRISb2_bit;
sbit GLCD_RW_Direction  at TRISb3_bit;
sbit GLCD_EN_Direction  at TRISb4_bit;
sbit GLCD_RST_Direction at TRISb5_bit;

// MMC module connections
sbit Mmc_Chip_Select           at LATC0_bit;  // for writing to output pin always use latch (PIC18 family)
sbit Mmc_Chip_Select_Direction at TRISC0_bit;
/*************************************************************************/
/********************** Methode Declaration Part ************************/
void adc_acc(unsigned int *ana_res,unsigned int ch_n)
{
  unsigned int i;
  for (i = 0 ; i < 128 ; i++)
 {
   ana_res[i] = adc_read(ch_n);
 }
}
//////////////////////////////////////////////////////////////
float y_step(unsigned int tp_y,unsigned int bp_y)
{
  float step;
  step = (1023 - 0)/(bp_y - tp_y);
  return step;
}
/////////////////////////////////////////////////////////////
void res_acc(unsigned int *cal_res,unsigned int *ana_res,float step)
{
  unsigned int i;
  for (i = 0 ; i < 128 ; i++)
 {
   cal_res[i] = 63-(ana_res[i]/step);
 }
}
/////////////////////////////////////////////////////////////
/*******************  Writing to SD Card *********************/
unsigned int mem_data_wr (unsigned int *ana_res)
{
 unsigned int thousands,hundreds,tens,ones,temp,i;
 const unsigned long size = 65536;
 char holder[6];
 unsigned long mmc_size;
 unsigned int re;
 Mmc_Fat_Assign("te_value.txt",0xA0);
 mmc_size = Mmc_Fat_Get_File_Size();
 if (mmc_size < size)
 {
  Mmc_Fat_Append();
  for (i = 0 ; i < 128 ; i++)
  {
      thousands = ana_res[i]/1000;
      temp = ana_res[i]%1000;
      hundreds = temp/100;
      temp = temp%100;
      tens = temp/10;
      ones = temp%10;
      holder[0] = thousands + 48;
      holder[1] = hundreds + 48;
      holder[2] = tens + 48;
      holder[3] = ones + 48;
      holder[4] = '\r';
      holder[5] = '\n';
      Mmc_Fat_Write(holder,6);
  }
  re = 1;
 } else
 {
    re = 0;
 }
   return re;
}
////////////////////////////////////////////////////////////
/*******************  reading from SD Card *********************/
void mem_data_re (unsigned int *mmc_res)
{
  unsigned long mmc_size,cur_pos;
  unsigned int temp[4],k,i,min = 0,max = 4;
  char character;
  Mmc_Fat_Assign("te_value.txt", 0);
 /*Mmc_Fat_Reset(&size);*/
  mmc_size = Mmc_Fat_Get_File_Size();
  cur_pos = Mmc_Fat_Tell();
  if (cur_pos < mmc_size)
  {
   for (i = 0; i < 128; i++)
   {
       for (k = min; k < max; k++)
       {
          Mmc_Fat_Read(&character);
          temp[k] = character - 48;
       }
       for (k = min; k < 2; k++)
        {
          Mmc_Fat_Read(&character);
        }
       mmc_res[i] = (temp[0] * 1000) + (temp[1] * 100) + (temp[2] * 10) + temp[3];
   }
  }else
  {
    Mmc_Fat_Reset(mmc_size);
  }
}
///////////////////////////////////////////////////////////
  void interpo (unsigned int past_i, unsigned int pres_i, unsigned int past_y,unsigned int pres_y)
  {
      unsigned int med, r_past_y , r_pres_y ,r_past_i, r_pres_i;
      if ( (pres_y - past_y) != 0 && (pres_y - past_y) != 1)
      {

        med = (pres_y - past_y)/2;
        if (med <0 )
        {
          med = abs(med);
          r_past_i = pres_i;
          r_pres_i = past_i;
          r_past_y = pres_y;
          r_pres_y = past_y;
  /*** On inverse les variables pour avoir des resultats correct***/
        }
        else   // Sinon on fait l'affectation direct
        {
         r_past_i = past_i;
         r_pres_i = pres_i;
         r_past_y = past_y;
         r_pres_y = pres_y;

        }
     Glcd_v_Line(r_past_y, r_past_y + med, r_past_i, 1);
     Glcd_v_Line(r_past_y + med,r_pres_y, r_pres_i, 1);
      }
      else
          {
          // Sinon au lieu de dessiner des lignes entre les points on dessign les points
      Glcd_Dot(pres_i, pres_y, 1);
      }
  } 

///////////////////////////////////////////////////////////
  void cur_plo (unsigned int *cal_res)
  {
  unsigned int i,past_y,pres_y;
  pres_y = cal_res[0];
     for(i = 1; i < 128; i++)
     {
      past_y = pres_y;
      pres_y = cal_res[i];
      interpo((i-1), i, past_y, pres_y);
     }
  }
//////////////////////////////////////////////////////////
/**************************** Main Function ***********************************/
void main()
{
 /********************** Var & Meth Init part *****************************/
 unsigned int err,ana_res[128],cal_res[128];
 unsigned int lp_x = 0,rp_x = 127,tp_y = 0,bp_y = 63;
 float step;
 ADCON1 = 0x02;
 PORTA = 0x00;
 TRISA = 1;
 PORTB.B6 = 1;
 PORTB.B7 = 1;
 TRISB.B6 = 1;
 TRISB.B7 = 1;
 Glcd_Init();
 Glcd_Fill(0x00);
 /************************** MMC Init routine *******************************/
  SPI1_Init_Advanced(_SPI_MASTER_OSC_DIV64, _SPI_DATA_SAMPLE_MIDDLE, _SPI_CLK_IDLE_LOW, _SPI_LOW_2_HIGH);
  err = MMC_Fat_Init();
  if (err == 1)
  {
     Glcd_Write_Text("MMC not FAT16", 1, 2, 1);
  } else if (err == 255)
  {
    Glcd_Write_Text("No MMC", 15, 2, 1);
  } else if (err == 0)
  {
    Glcd_Write_Text("MMC Detected", 15, 2, 1);
    SPI1_Init_Advanced(_SPI_MASTER_OSC_DIV4, _SPI_DATA_SAMPLE_MIDDLE, _SPI_CLK_IDLE_LOW, _SPI_LOW_2_HIGH);
  }
  delay_ms(500);
 /**************************************************************************/  
 step = y_step(tp_y,bp_y);
 while(1)
 {
  if (PORTB.B6 == 0)
  {
    mem_data_re(ana_res);
    res_acc(cal_res,ana_res,step);
  }else if (PORTB.B6 == 1)
  {
   adc_acc(ana_res,0);
   res_acc(cal_res,ana_res,step);
   if (PORTB.B7 == 0)
   {
      err = mem_data_wr(ana_res);
      Glcd_Fill(0x00);
      if (err = 0)
      {
         Glcd_Write_Text("Error", 30, 2, 1);
      }else if (err = 1)
      {
         Glcd_Write_Text("Done", 30, 2, 1);
      }
      delay_ms(500);
   }

  }
  Glcd_Fill(0x00);
  cur_plo(cal_res);
  delay_ms(500);
 }
}
/******************** End Of Main Source Code **********************/
