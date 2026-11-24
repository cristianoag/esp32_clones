static unsigned wantedMappers[2];
static std::string mapperPaths[2], inspectProfile;
static int wantedDetection;

static void checkMapperOverrides()
{
  const word pc=CPU.PC.W;
  const int mode=Mode, chunks=NChunks;
  const unsigned previousErrors=errors;
  const byte ps=PSLReg;
  byte banks[2][4];
  memcpy(banks,ROMMapper,sizeof(banks));
  for(unsigned slot=0;slot<2;++slot)
  {
    assert(ROMType[slot]==wantedMappers[slot] && ROMMask[slot]>=3);
    MsxCartridgeInfo info={-123,"unchanged"};
    char error[160];
    assert(MsxInspectCartridge(mapperPaths[slot].c_str(),inspectProfile.c_str(),info,error,sizeof(error)));
    assert(info.detectedMapper==wantedDetection && info.source && !error[0]);
  }
  assert(CPU.PC.W==pc && Mode==mode && NChunks==chunks && errors==previousErrors && !ExitNow);
  assert(!memcmp(banks,ROMMapper,sizeof(banks)));
  for(unsigned slot=0;slot<2;++slot)
  {
    OutZ80(0xA8,0xC0|((slot+1)*0x14));
    const word addresses[]={0xA000,0x8000,0xB000,0xA000,0x7800,0x77FF,0xA000,0x7FF7};
    const unsigned mapper=wantedMappers[slot], bank=ROMMask[slot]-1;
    const bool wide=mapper==MAP_GEN16||mapper==MAP_ASCII16||mapper==MAP_FMPAC;
    WrZ80(addresses[mapper],wide?bank/2:bank);
    const word address=mapper==MAP_FMPAC?0x4100:wide?0x8100:0xA100;
    assert(RdZ80(address)==ROMData[slot][bank*8192+0x100]);
  }
  OutZ80(0xA8,ps);
}

static void mapperOverrideRegression(const char* directory)
{
  writeImage("MSX.ROM",slotScanningBios());
  mapperPaths[0]=std::string(directory)+"/mapper-a.rom";
  mapperPaths[1]=std::string(directory)+"/mapper-b.rom";
  inspectProfile=directory;
  mapperOverrideTest=true;
  for(unsigned size : {8192U,16384U,32768U,49152U,131072U})
  {
    const auto image=makeCartridge(size,0x40,MAP_KONAMI5);
    for(const auto& path : mapperPaths) writeImage(path.c_str(),image);
    wantedDetection=size<=32768?-1:MAP_KONAMI5;
    for(unsigned mapper=0;mapper<8;++mapper)
    {
      wantedMappers[0]=mapper;
      wantedMappers[1]=7-mapper;
      resetCounters();
      assert(MsxCoreRun(directory,0,4,mapperPaths[0].c_str(),mapperPaths[1].c_str(),
                        nullptr,nullptr,nullptr,wantedMappers[0],wantedMappers[1]));
      assert(!errors&&frames==frameLimit&&!NChunks);
    }
  }
  mapperOverrideTest=false;
  for(unsigned invalid : {9U,255U})
  {
    resetCounters();
    assert(!MsxCoreRun(directory,0,4,mapperPaths[0].c_str(),nullptr,nullptr,nullptr,nullptr,invalid));
    assert(!frames&&errors==1);
  }
  const auto image=makeCartridge(131072,0x40,MAP_KONAMI5);
  for(const auto& path : mapperPaths) writeImage(path.c_str(),image);
#ifdef _WIN32
  assert(mkdir("mapper-profile")==0);
#else
  assert(mkdir("mapper-profile",0700)==0);
#endif
  inspectProfile=std::string(directory)+"/mapper-profile";
  unsigned sum=0;
  for(byte value:image) sum+=value;
  FILE* database=fopen("mapper-profile/CARTS.CRC","wb");
  assert(database);
  fprintf(database,"%08X %d\n",sum,MAP_ASCII8);
  assert(!fclose(database));
  wantedDetection=MAP_ASCII8;
  wantedMappers[0]=MAP_KONAMI5; wantedMappers[1]=MAP_KONAMI4;
  mapperOverrideTest=true;
  resetCounters();
  assert(MsxCoreRun(directory,0,4,mapperPaths[0].c_str(),mapperPaths[1].c_str(),
                    nullptr,nullptr,nullptr,MsxMapperAuto,MsxKonami));
  mapperOverrideTest=false;
  assert(!errors&&!NChunks);

  MsxCartridgeInfo info={-123,"unchanged"};
  char error[160];
  resetCounters();
  failAllocation=1;
  assert(!MsxInspectCartridge(mapperPaths[0].c_str(),directory,info,error,sizeof(error)));
  assert(info.detectedMapper==-123&&strstr(error,"PSRAM")&&!errors&&!NChunks);
  failAllocation=0;
  assert(!MsxInspectCartridge(nullptr,directory,info,error,sizeof(error))&&error[0]);
  assert(!MsxInspectCartridge(mapperPaths[0].c_str(),"relative",info,error,sizeof(error)));

  const std::string shortPath=std::string(directory)+"/inspection.rom";
  writeImage(shortPath.c_str(),image);
  truncateInspectionOnAllocation=true;
  assert(!MsxInspectCartridge(shortPath.c_str(),directory,info,error,sizeof(error)));
  assert(!truncateInspectionOnAllocation && info.detectedMapper==-123 && !errors && !NChunks);
  assert(remove(shortPath.c_str())==0);
  database=fopen("mapper-profile/CARTS.CRC","wb");
  assert(database);
  fprintf(database,"%08X 99\n",sum);
  assert(!fclose(database));
  assert(!MsxInspectCartridge(mapperPaths[0].c_str(),inspectProfile.c_str(),info,error,sizeof(error)));
  assert(strstr(error,"invalid mapper")&&info.detectedMapper==-123);
  assert(remove("mapper-profile/CARTS.CRC")==0);
  assert(rmdir("mapper-profile")==0);
  for(const auto& path:mapperPaths) assert(remove(path.c_str())==0);
  puts("PASS: safe live mapper inspection, selected-profile overrides, both-slot manual/Auto boot, all eight mappers on small/large ROMs, invalid IDs and failed-read/OOM recovery.");
}
