// Synthetic media only. Included in host_smoke.cpp to share lifecycle/fault injection.
extern "C" {
extern WD1793 FDC;
void SSlot(byte);
byte DiskRead(byte,byte*,int);
byte DiskWrite(byte,const byte*,int);
}

static std::string mediaDirectory;
static std::vector<byte> diskImages[2], tapeImage;
static bool mediaBiosPresent;
static bool mediaCpuHooks;
static int failDiskIo;

extern "C" size_t fmsxTestDiskWrite(const void *data,size_t size,size_t count,FILE *file)
{
  if(failDiskIo==1) { const size_t result=fwrite(data,size,count/2,file);errno=ENOSPC;return result; }
  return fwrite(data,size,count,file);
}
extern "C" int fmsxTestDiskFlush(FILE *file)
{
  if(failDiskIo==2) { errno=EIO;return EOF; }
  return fflush(file);
}
extern "C" int fmsxTestDiskSync(int fd)
{
  if(failDiskIo==3) { errno=EIO;return -1; }
#ifdef _WIN32
  return _commit(fd);
#else
  return fsync(fd);
#endif
}

static void assertDiskFile(unsigned drive)
{
  FILE* file=fopen((mediaDirectory+(drive?"/media-b.dsk":"/media-a.dsk")).c_str(),"rb");
  std::vector<byte> data(diskImages[drive].size());
  assert(file && fread(data.data(),1,data.size(),file)==data.size() && fgetc(file)==EOF);
  assert(!fclose(file) && data==diskImages[drive]);
}

static void prepareRealMedia()
{
  for (unsigned drive=0;drive<2;++drive) {
    std::vector<byte> disk(drive?737280:368640);
    disk[0]=0xEB;disk[1]=0xFE;disk[2]=0x90;
    memcpy(disk.data()+3,"MSXTEST ",8);
    disk[12]=2;disk[13]=2;disk[14]=1;disk[16]=2;disk[17]=112;
    const unsigned sectors=drive?1440:720, fat=drive?3:2;
    disk[19]=sectors&255;disk[20]=sectors>>8;disk[21]=drive?0xF9:0xF8;
    disk[22]=fat;disk[24]=9;disk[26]=drive?2:1;
    disk[30]=0xC9; // Non-bootable synthetic disk returns from boot entry.
    for (unsigned copy=0;copy<2;++copy) {
      const unsigned offset=(1+copy*fat)*512;
      disk[offset]=disk[21];disk[offset+1]=disk[offset+2]=0xFF;
      disk[offset+3]=0xFF;disk[offset+4]=0x0F; // Cluster 2: EOF.
    }
    const unsigned root=(1+2*fat)*512;
    memcpy(disk.data()+root,drive?"DISKB   TXT":"DISKA   TXT",11);
    disk[root+26]=2;disk[root+28]=1;
    disk[root+7*512]='X';
    writeImage(drive?"real-b.dsk":"real-a.dsk",disk);
  }
  const byte header[]={0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74};
  std::vector<byte> cas(header,header+8);
  cas.insert(cas.end(),10,0xD0);
  const char* name="MSXTST";
  cas.insert(cas.end(),name,name+6);
  cas.insert(cas.end(),header,header+8);
  cas.insert(cas.end(),{0x00,0xC7,0x00,0xC7,0x00,0xC7,0x5A});
  writeImage("real.cas",cas);
}

static void injectMediaCommand()
{
  const char* initial[]={"FILES \"A:\"\r","FILES \"B:\"\r","BLOAD \"CAS:\"\r",
    "IF PEEK(&HC700)=90 THEN ?\"TAPE\";\"OK\"\r",
    "10 PRINT \"SAVED\";\"AOK\"\r","SAVE \"A:SAVED.BAS\"\r",
    "10 PRINT \"SAVED\";\"BOK\"\r","SAVE \"B:SAVED.BAS\"\r"};
  const char* reload[]={"LOAD \"A:SAVED.BAS\"\r","RUN\r","NEW\r","LOAD \"B:SAVED.BAS\"\r","RUN\r"};
  const char** commands=realMediaReload?reload:initial;
  const unsigned count=realMediaReload?5:8;
  if (mediaCommands>=count||frames<frameLimit-480+mediaCommands*50) return;
  const byte ps=PSLReg,ss=SSLReg[3];
  OutZ80(0xA8,0xFF);SSlot(0xAA);
  // Only queue a command when the BIOS keyboard ring is empty.
  const word put=RdZ80(0xF3F8)|(RdZ80(0xF3F9)<<8);
  const word get=RdZ80(0xF3FA)|(RdZ80(0xF3FB)<<8);
  if(put==get) {
    const char* command=commands[mediaCommands++];
    const unsigned length=strlen(command);
    assert(length<40);
    for(unsigned i=0;i<length;++i) WrZ80(0xFBF0+i,command[i]);
    WrZ80(0xF3FA,0xF0);WrZ80(0xF3FB,0xFB);
    WrZ80(0xF3F8,(0xFBF0+length)&255);WrZ80(0xF3F9,(0xFBF0+length)>>8);
  }
  SSlot(ss);OutZ80(0xA8,ps);
}

static void assertSavedProgram(const char* path)
{
  FILE* file=fopen(path,"rb");
  assert(file);
  assert(!fseek(file,0,SEEK_END));
  const long size=ftell(file);
  assert(size==368640||size==737280);
  rewind(file);
  std::vector<byte> data(size);
  assert(fread(data.data(),1,data.size(),file)==data.size()&&!fclose(file));
  const unsigned fat=data[22]|(data[23]<<8), root=(1+2*fat)*512;
  bool found=false;
  for(unsigned offset=root;offset<root+112*32;offset+=32)
    if(!memcmp(data.data()+offset,"SAVED   BAS",11))
    {
      const unsigned cluster=data[offset+26]|(data[offset+27]<<8);
      const unsigned length=data[offset+28]|(data[offset+29]<<8);
      assert(cluster>=2 && length>10 && length<512);
      const unsigned start=root+7*512+(cluster-2)*1024;
      assert(start+length<=data.size() && data[start]==0xFF);
      found=true;
    }
  assert(found);
}

static Z80 mediaTrap(word address, byte drive = 0, bool write = false,
                     word sector = 1, byte count = 1, byte descriptor = 0xF9)
{
  Z80 r = {};
  r.PC.W = address + 2;
  r.AF.B.h = drive;
  r.AF.B.l = write ? C_FLAG : 0;
  r.BC.B.h = count;
  r.BC.B.l = descriptor;
  r.DE.W = sector;
  r.HL.W = 0xC800;
  PatchZ80(&r);
  return r;
}

static void checkMedia()
{
  char error[160];
  const std::string a = mediaDirectory + "/media-a.dsk";
  const std::string b = mediaDirectory + "/media-b.dsk";
  const std::string cas = mediaDirectory + "/media.cas";
  const std::string bad = mediaDirectory + "/media-bad.bin";
  assert(MsxDiskAvailable() == mediaBiosPresent);
  const byte ps = PSLReg, ss = SSLReg[3];
  OutZ80(0xAB,0x82);
  OutZ80(0xA8, 0xFC);
  SSlot(0xAE); // RAM pages 2/3; disk BIOS page 1, main BIOS page 0.
  const unsigned previousErrors = errors;
  if (mediaCpuHooks) {
    assert(RdZ80(0xC100) == 0x42);
    if (mediaBiosPresent) {
      assert(!(RdZ80(0xC102)&C_FLAG) && RdZ80(0xC103)==0);
      assert(RdZ80(0xC900)==diskImages[0][512]);
      assert(!(RdZ80(0xC104)&C_FLAG) && RdZ80(0xC105)==1);
      assert(RdZ80(0xCB00)==diskImages[1][512]);
    }
    assert(MsxRewindTape(error,sizeof(error)));
  }
  if (!mediaBiosPresent) {
    assert(!MsxAttachDisk(0,a.c_str(),error,sizeof(error)));
    assert(strstr(error,"DISK.ROM") && strstr(error,"cold boot"));
    assert(MsxAttachDisk(0,"",error,sizeof(error)));
  } else {
    assert(FDD[0].Data && FDD[1].Data && !FDD[0].Data[3] && !FDD[1].Data[3]);
    assert(MemMap[3][3][2][0x10] == 0xED);
    for (unsigned drive = 0; drive < 2; ++drive) {
      Z80 r = mediaTrap(0x4010,drive);
      if ((r.AF.B.l&C_FLAG) || r.BC.B.h)
        fprintf(stderr,"Disk trap: model=%d AF=%04X BC=%04X PSL=%02X SSL=%02X mapped=%d present=%d geometry=%d/%d/%d\n",
                Mode&MSX_MODEL,r.AF.W,r.BC.W,PSLReg,SSLReg[3],
                RAM[2]==MemMap[3][3][2],DiskROMAvailable(),FDD[drive].Sides,FDD[drive].Tracks,FDD[drive].Sectors);
      assert(!(r.AF.B.l&C_FLAG) && !r.BC.B.h);
      for (unsigned i = 0; i < 512; ++i)
        assert(RdZ80(0xC800+i) == diskImages[drive][512+i]);
      for(unsigned i=0;i<1024;++i) WrZ80(0xC800+i,byte(i+drive));
      r = mediaTrap(0x4010,drive,true,10,2);
      assert(!(r.AF.B.l&C_FLAG) && !r.BC.B.h);
      for(unsigned i=0;i<1024;++i) diskImages[drive][10*512+i]=byte(i+drive);
      assertDiskFile(drive); // Read through a second handle before ejection.
      r = mediaTrap(0x4010,drive,false,1440);
      assert((r.AF.B.l&C_FLAG) && r.AF.B.h == 8);
      r = mediaTrap(0x4010,drive,false,0,1,0);
      assert((r.AF.B.l&C_FLAG) && r.AF.B.h == 8);
      r = mediaTrap(0x4013,drive);
      assert(!(r.AF.B.l&C_FLAG) && r.BC.B.h == 0);
      r = mediaTrap(0x4016,drive);
      assert(!(r.AF.B.l&C_FLAG));
      r = mediaTrap(0x401C,1,false,drive<<8);
      assert((r.AF.B.l&C_FLAG) && r.AF.B.h==12);
      byte buf[512] = {};
      assert(DiskWrite(drive,buf,12));
      memset(diskImages[drive].data()+12*512,0,512);
      assert(!DiskWrite(drive,buf,-1));
      assert(!DiskWrite(drive,buf,diskImages[drive].size()/512));
      assert(!SaveFDI(&FDD[drive], "media-forbidden.dsk", FMT_MSXDSK));
      Write1793(&FDC,WD1793_SYSTEM,drive|S_DENSITY|S_SIDE);
      Write1793(&FDC,WD1793_COMMAND,0xD0);
      Write1793(&FDC,WD1793_TRACK,0);
      Write1793(&FDC,WD1793_SECTOR,2);
      Write1793(&FDC,WD1793_COMMAND,0xA0);
      for(unsigned i=0;i<100;++i) Write1793(&FDC,WD1793_DATA,0x77);
      Write1793(&FDC,WD1793_COMMAND,0xD0); // Interrupted sectors never reach RAM or SD.
      assertDiskFile(drive);
      Write1793(&FDC,WD1793_SECTOR,8);
      Write1793(&FDC,WD1793_COMMAND,0xB0);
      for(unsigned i=0;i<1024;++i) Write1793(&FDC,WD1793_DATA,byte(i+7));
      assert(!FDC.WRLength && !(FDC.R[0]&(F_BUSY|F_WRFAULT)));
      for(unsigned i=0;i<1024;++i) diskImages[drive][7*512+i]=byte(i+7);
      assertDiskFile(drive);
      Write1793(&FDC,WD1793_COMMAND,0xF0);
      assert(FDC.R[0]&F_WRFAULT);
      FDD[drive].Data[3]=1;
      r=mediaTrap(0x4010,drive,true);
      assert((r.AF.B.l&C_FLAG) && !r.AF.B.h && r.BC.B.h==1);
      Write1793(&FDC,WD1793_COMMAND,0xA0);
      assert(FDC.R[0]&F_READONLY);
      FDD[drive].Data[3]=0;
      assert(!memcmp(DataFDI(&FDD[drive]),diskImages[drive].data(),diskImages[drive].size()));
    }
    assert(access("media-forbidden.dsk",F_OK) != 0);
    byte* old = FDD[0].Data;
    assert(!MsxAttachDisk(2,a.c_str(),error,sizeof(error)) && error[0]);
    assert(!MsxAttachDisk(0,bad.c_str(),error,sizeof(error)) && FDD[0].Data == old);
    assert(!ChangeDisk(0,bad.c_str()) && FDD[0].Data == old);
    writeImage("media-short.dsk",diskImages[0]);
    const std::string shortPath=mediaDirectory+"/media-short.dsk";
    truncateMediaOnAllocation=true;
    assert(!MsxAttachDisk(0,shortPath.c_str(),error,sizeof(error)));
    assert(!truncateMediaOnAllocation && FDD[0].Data==old);
    assert(remove("media-short.dsk")==0);
    failAllocation = allocation + 1;
    assert(!MsxAttachDisk(0,a.c_str(),error,sizeof(error)) && FDD[0].Data == old);
    failAllocation = 0;
    assert(!MsxAttachDisk(0,b.c_str(),error,sizeof(error)) && strstr(error,"another drive"));
    assert(MsxAttachDisk(1,"",error,sizeof(error)));
    assert(MsxAttachDisk(0,b.c_str(),error,sizeof(error)) && !error[0]);
    assert(!memcmp(DataFDI(&FDD[0]),diskImages[1].data(),diskImages[1].size()));
    assert(MsxAttachDisk(0,"",error,sizeof(error)) && !FDD[0].Data);
    Z80 r = mediaTrap(0x4010);
    assert((r.AF.B.l&C_FLAG) && r.AF.B.h == 2);
    assert(MsxAttachDisk(0,a.c_str(),error,sizeof(error)));
    assert(MsxAttachDisk(1,nullptr,error,sizeof(error)) && !FDD[1].Data);
    assert(MsxAttachDisk(1,b.c_str(),error,sizeof(error)));
    for(int fault=1;fault<=3;++fault)
    {
      byte before[512];
      memcpy(before,LinearFDI(&FDD[0],20),512);
      for(unsigned i=0;i<512;++i) WrZ80(0xC800+i,0xE7);
      failDiskIo=fault;
      r=mediaTrap(0x4010,0,true,20);
      assert((r.AF.B.l&C_FLAG) && r.AF.B.h==10 && r.BC.B.h==1);
      assert(FDD[0].WriteFault && !memcmp(before,LinearFDI(&FDD[0],20),512));
      failDiskIo=0;
      assert(!DiskWrite(0,before,20)); // Fault remains latched until reattachment.
      assert(MsxAttachDisk(0,"",error,sizeof(error)));
      writeImage("media-a.dsk",diskImages[0]);
      assert(MsxAttachDisk(0,a.c_str(),error,sizeof(error)) && !FDD[0].WriteFault);
    }
    Write1793(&FDC,WD1793_SYSTEM,S_DENSITY|S_SIDE);
    Write1793(&FDC,WD1793_TRACK,0);
    Write1793(&FDC,WD1793_SECTOR,2);
    Write1793(&FDC,WD1793_COMMAND,0xA0);
    failDiskIo=3;
    for(unsigned i=0;i<512;++i) Write1793(&FDC,WD1793_DATA,0xE8);
    assert((FDC.R[0]&F_WRFAULT) && !FDC.WRLength && FDD[0].WriteFault);
    failDiskIo=0;
    assert(MsxAttachDisk(0,"",error,sizeof(error)));
    writeImage("media-a.dsk",diskImages[0]);
    assert(MsxAttachDisk(0,a.c_str(),error,sizeof(error)));
    // The same virtual address in an unrelated slot must not invoke disk I/O.
    OutZ80(0xA8,0xF4);
    r = mediaTrap(0x4010);
    assert(r.BC.B.h == 1);
    OutZ80(0xA8,0xFC);
  }
  assert(CasStream);
  Z80 r = mediaTrap(0x00E1);
  assert(!(r.AF.B.l&C_FLAG));
  r = mediaTrap(0x00E4);
  assert(!(r.AF.B.l&C_FLAG) && r.AF.B.h == 0x42);
  FILE* oldTape = CasStream;
  const long position = ftell(CasStream);
  assert(!MsxAttachTape(bad.c_str(),error,sizeof(error)));
  assert(CasStream == oldTape && ftell(CasStream) == position);
  assert(!ChangeTape(bad.c_str()) && CasStream == oldTape);
  // A second marker deliberately starts at an unaligned offset.
  r = mediaTrap(0x00E1);
  assert(!(r.AF.B.l&C_FLAG));
  r = mediaTrap(0x00E4);
  assert(!(r.AF.B.l&C_FLAG) && r.AF.B.h == 0x99);
  for (unsigned i = 0; i < 2; ++i) {
    r = mediaTrap(0x00E4);
    assert(r.AF.B.l&C_FLAG); // EOF stays at EOF until explicit rewind.
  }
  assert(MsxRewindTape(error,sizeof(error)) && !error[0]);
  assert(ftell(CasStream) == 0);
  for (word hook : {word(0xEA),word(0xED),word(0xF0)}) {
    r = mediaTrap(hook);
    assert(r.AF.B.l&C_FLAG);
    assert(ftell(CasStream) == 0);
  }
  assert(MsxAttachTape("",error,sizeof(error)) && !CasStream);
  assert(!MsxRewindTape(error,sizeof(error)) && error[0]);
  r = mediaTrap(0xE4);
  assert(r.AF.B.l&C_FLAG);
  assert(MsxAttachTape(cas.c_str(),error,sizeof(error)));
  assert(errors == previousErrors && !ExitNow);
  SSlot(ss);
  OutZ80(0xA8,ps);
}

static void mediaRegression(const char* directory)
{
  mediaDirectory = directory;
  diskImages[0] = std::vector<byte>(368640,0x3A);
  diskImages[1] = std::vector<byte>(737280,0x6B);
  // An AB prefix is valid raw data, not a cartridge-type discriminator.
  diskImages[0][0] = 'A'; diskImages[0][1] = 'B';
  const byte marker[] = {0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74};
  tapeImage.assign(marker,marker+8);
  tapeImage.push_back(0x42);
  tapeImage.insert(tapeImage.end(),marker,marker+8);
  tapeImage.push_back(0x99);
  writeImage("media-a.dsk",diskImages[0]);
  writeImage("media-b.dsk",diskImages[1]);
  writeImage("media.cas",tapeImage);
  writeImage("media-bad.bin",std::vector<byte>(17));
  const std::string a = mediaDirectory + "/media-a.dsk";
  const std::string b = mediaDirectory + "/media-b.dsk";
  const std::string cas = mediaDirectory + "/media.cas";
  char error[128];
  assert(MsxValidateDisk(nullptr,error,sizeof(error)) && !error[0]);
  assert(MsxValidateTape("",error,sizeof(error)) && !error[0]);
  assert(MsxValidateDisk(a.c_str(),error,sizeof(error)));
  assert(MsxValidateDisk(b.c_str(),error,sizeof(error)));
  assert(MsxValidateTape(cas.c_str(),error,sizeof(error)));
  assert(!MsxValidateTape(a.c_str(),error,sizeof(error)));
  assert(!MsxValidateDisk("relative.dsk",error,sizeof(error)));
  assert(!MsxDiskAvailable());
  const char* mainNames[]={"MSX.ROM","MSX2.ROM","MSX2P.ROM"};
  std::vector<byte> savedMain[3];
  for (unsigned i=0;i<3;++i) {
    FILE* f=fopen(mainNames[i],"rb");
    savedMain[i].resize(32768);
    assert(f && fread(savedMain[i].data(),1,32768,f)==32768);
    fclose(f);
  }
  auto cpuBios = [&](bool withDisk) {
    std::vector<byte> bios(32768);
    std::vector<byte> code={
      0xF3,0x31,0x00,0xF0,0x3E,0x82,0xD3,0xAB,
      0x3E,0xFC,0xD3,0xA8,0x3E,0xAE,0x32,0xFF,0xFF,
      0xCD,0xE1,0x00,0xCD,0xE4,0x00,0x32,0x00,0xC1
    };
    if (withDisk) {
      const byte diskCode[]={
        0xAF,0x01,0xF9,0x01,0x11,0x01,0x00,0x21,0x00,0xC9,
        0xCD,0x10,0x40,0xF5,0xE1,0x22,0x02,0xC1,
        0xAF,0x3E,0x01,0x01,0xF9,0x01,0x11,0x01,0x00,0x21,0x00,0xCB,
        0xCD,0x10,0x40,0xF5,0xE1,0x22,0x04,0xC1
      };
      code.insert(code.end(),diskCode,diskCode+sizeof(diskCode));
    }
    const byte halt=code.size();
    code.insert(code.end(),{0xF3,0x76,0xC3,halt,0x00});
    memcpy(bios.data(),code.data(),code.size());
    for (const char* name : mainNames) writeImage(name,bios);
  };
  mediaTest = true;
  mediaCpuHooks = true;
  mediaBiosPresent = false;
  cpuBios(false);
  resetCounters();
  assert(MsxCoreRun(directory,0,4,nullptr,nullptr,nullptr,nullptr,cas.c_str()));
  resetCounters();
  assert(!MsxCoreRun(directory,0,4,nullptr,nullptr,a.c_str()));
  assert(!frames && errors == 1 && strstr(lastError,"DISK.ROM"));
  std::vector<byte> diskBios(16384);
  diskBios[0]='A'; diskBios[1]='B';
  for (unsigned entry : {16U,19U,22U,28U,31U}) diskBios[entry]=0xC3;
  writeImage("DISK.ROM",diskBios);
  mediaBiosPresent = true;
  cpuBios(true);
  for (int model : {0,1,2}) {
    resetCounters();
    assert(MsxCoreRun(directory,model,4,nullptr,nullptr,a.c_str(),b.c_str(),cas.c_str()));
    assert(!errors && frames == frameLimit && !NChunks && !CasStream);
    assert(!FDD[0].Data && !FDD[1].Data && !MsxDiskAvailable());
  }
  mediaTest = false;
  mediaCpuHooks = false;
  resetCounters();
  assert(MsxCoreRun(directory,0,4,nullptr,nullptr,a.c_str(),b.c_str(),cas.c_str()));
  const unsigned bootAllocations=allocation;
  // Required disk images must not silently boot ejected after an OOM.
  for (unsigned failure=bootAllocations-1;failure<=bootAllocations;++failure) {
    resetCounters();
    failAllocation=failure;
    assert(!MsxCoreRun(directory,0,4,nullptr,nullptr,a.c_str(),b.c_str(),cas.c_str()));
    assert(!frames && errors==1 && !NChunks && !FDD[0].Data && !FDD[1].Data && !CasStream);
  }
  failAllocation=0;
  for (unsigned i=0;i<3;++i) writeImage(mainNames[i],savedMain[i]);
  for (unsigned drive = 0; drive < 2; ++drive) {
    FILE* f=fopen(drive ? b.c_str() : a.c_str(),"rb");
    std::vector<byte> contents(diskImages[drive].size());
    assert(f && fread(contents.data(),1,contents.size(),f)==contents.size());
    fclose(f);
    assert(contents==diskImages[drive]);
  }
  FILE* f=fopen(cas.c_str(),"rb");
  std::vector<byte> contents(tapeImage.size());
  assert(f && fread(contents.data(),1,contents.size(),f)==contents.size());
  fclose(f);
  assert(contents==tapeImage);
  for (const char* name : {"DISK.ROM","media-a.dsk","media-b.dsk","media.cas","media-bad.bin"})
    assert(remove(name)==0);
  puts("PASS: persistent A/B writes, BIOS/FDC sectors, partial/flush/sync failures, duplicate mounts, atomic swaps, eject/reopen, and read-only CAS.");
}
