# --- Common variables  -------------------------------------------------------------

INCLUDES = 	-i "{DDK_Includes-dir}" ¶
	-i "{DDK_Includes-dir}CLibrary:" ¶
	-i "{DDK_Includes-dir}CommAPI:" ¶
	-i "{DDK_Includes-dir}Communications:" ¶
	-i "{DDK_Includes-dir}Frames:" ¶
	-i "{DDK_Includes-dir}HAL:" ¶
	-i "{DDK_Includes-dir}OS600:" ¶
	-i "{DDK_Includes-dir}Power:" ¶
	-i "{DDK_Includes-dir}Toolbox:" ¶
	-i "{DDK_Includes-dir}UtilityClasses:" ¶
	-i "{DDK_Includes-dir}PCMCIA:" ¶
	-i "{NCT_Includes}" ¶
	-i "{NCT_Includes}Frames:" ¶
	-i "{NCT_Includes}Utilities:"
LIBS = "{NCT_Libraries}MP2x00US.a.o"

MAKEFILE     = Makefile
Objects-dir  = :{NCT-ObjectOut}:
LIB          = {NCT-lib} {NCT-lib-options} {LocalLibOptions} 
LINK         = {NCT-link}
LINKOPTS     = {NCT-link-options} -rel {LocalLinkOptions} 
Asm          = {NCT-asm} {NCT-asm-options} {LocalAsmOptions}
CFront       = {NCT-cfront} {NCT-cfront-options} {LocalCFrontOptions}
CFrontC      = {NCT-cfront-c} {NCT-cfront-c-options} {LocalCfrontCOptions}
C            = {NCT-ARMc} {NCT-ARMc-options} 
ARMCPlus     = {NCT-ARMCpp} {NCT-ARMCpp-options} {LocalARMCppOptions}
ProtocolOptions = -package
Pram         = {NCT-pram} {NCT-pram-options} {LocalPRAMOptions} 
SETFILE      = {NCT-setfile-cmd}
SETFILEOPTS  = 
LocalLinkOptions = -dupok -debug
LocalARMCppOptions = -cfront -W -gf
LocalCfronttOptions = 
LocalCfrontCOptions = -W 
LocalCOptions = -d forARM 
LocalPackerOptions =  -packageid 'rfcm' -copyright 'Copyright (c) 2003 Eckhart Kšppen'
COUNT        = Count
COUNTOPTS    = 
CTAGS        = CTags
CTAGSOPTS    = -local -update
DELETE       = Delete
DELETEOPTS   = -i
FILES        = Files
FILESOPTS    = -l
LIBOPTS      = 
PRINT        = Print
PRINTOPTS    = 
REZ          = Rez

AOptions = -i "{DDK_Includes-dir}"

POptions = -i "{DDK_Includes-dir}"

ROptions = -i "{DDK_Includes-dir}" -a

COptions = {INCLUDES} {NCT_DebugSwitch} {LocalCOptions}
	
ASFLAGS = "{NCT-asm-options}"

all Ä BluntServer.ntkc BluntTool.pkg

# --- Blunt Server ------------------------------------------------------------------

SERVER_EXPORTS = Exports.exp
SERVER_SRCS = ¶
	Main.cp BluntServer.cp BluntClient.cp HCI.cp L2CAP.cp SDP.cp RFCOMM.cp EventsCommands.cp Logger.cp {EXPORTS}
SERVER_OBJS = ¶
	Main.cp.o BluntServer.cp.o BluntClient.cp.o HCI.cp.o ¶
	L2CAP.cp.o SDP.cp.o RFCOMM.cp.o EventsCommands.cp.o Logger.cp.o Exports.exp.o
SERVER_HDRS = ¶
	BluntClient.h CircleBuf.h EventsCommands.h Logger.h RFCOMMTool.h ¶
    BluntServer.h CommToolImpl.h HCI.h RFCOMM.h SDP.h ¶
    Buffer.h Definitions.h L2CAP.h RFCOMMService.impl.h

BluntServer.ntkc Ä {SERVER_OBJS}
	{NCT-link} {LINKOPTS} -dupok -o {Targ} {Deps} {LIBS}
	Rename -y {Targ} BluntServer.sym
	{NCT-AIFtoNTK} {LocalAIFtoNTKOptions} -via "{SERVER_EXPORTS}" -o  {Targ} BluntServer.sym
	Rez "{NCTTools}NCTIcons.r" -i "{RIncludes}" -append -o {Targ}
	{SETFILE} {SETFILEOPTS} {Targ}
	
# --- Blunt CommTool ---------------------------------------------------------------

TOOL_OBJS = ¶
	RFCOMMService.cp.o RFCOMMTool.cp.o RelocHack.a.o RFCOMMService.impl.h.o EventsCommands.cp.o
TOOL_SRCS = ¶
	RFCOMMService.cp RFCOMMTool.cp RelocHack.a RFCOMMService.impl.h EventsCommands.cp
TOOL_HDRS = RFCOMMTool.h
PACKERFLAGS = -packageid 'rfcm' -version 1 -copyright 2005

BluntTool.pkg Ä {TOOL_OBJS}
	{NCT-link} {NCT-link-options} -o BluntTool.bin {Deps} {LIBS}
	{NCT-packer} -o {Targ} BluntTool {PACKERFLAGS}  -protocol -aif BluntTool.bin -autoLoad -autoRemove
	Delete -i BluntTool.bin
	{SETFILE}  -t "pkg " -c "pkgX" {Targ}


OBJS = {SERVER_OBJS} {TOOL_OBJS}

# --- Generic rules ----------------------------------------------------------------

clean	Ä
	{DELETE} {DELETEOPTS} {OBJS} {MODULE} {SYMFILE} {AIFFILE}

.cp.o		Ä		.cp
	{ARMCPlus}	{depDir}{Default}.cp {COptions} -o {targDir}{Default}.cp.o

.cf.o		Ä		.cf
	{CFront} {depDir}{Default}.cf {COptions} {NCT-cfront-redirection} "{{CPlusScratch}}"X{Default}.cf -o {targDir}{Default}.cf.o
	{CFrontC} "{{CPlusScratch}}"X{Default}.cf -o {targDir}{Default}.cf.o  ; Delete -i "{{CPlusScratch}}"X{Default}.cf

.c.o	    Ä		.c
	{C} {depDir}{Default}.c -o {targDir}{Default}.c.o {COptions}

.exp.o		Ä		.exp
	"{NCTTools}"NCTBuildMain	{depDir}{Default}.exp "{{CPlusScratch}}"{Default}.exp.main.a
	{Asm}	"{{CplusScratch}}"{Default}.exp.main.a -o {targDir}{Default}.exp.o ; Delete -i "{{CPlusScratch}}"{Default}.exp.main.a

.a.o		Ä		.a
	{Asm}	{depDir}{Default}.a  -o {targDir}{Default}.a.o {AOptions}

.h.o		Ä		.h
	ProtocolGen -InterfaceGlue {depDir}{Default}.h {COptions} -stdout > "{{CPlusScratch}}"{Default}.glue.a
	{Asm} "{{CPlusScratch}}"{Default}.glue.a -o {targDir}{Default}.h.o ; Delete -i "{{CPlusScratch}}"{Default}.glue.a

.impl.h.o	Ä		.impl.h
	ProtocolGen -ImplementationGlue {depDir}{Default}.impl.h {ProtocolOptions} {COptions} -stdout >"{{CPlusScratch}}"{Default}.impl.a
	{Asm} "{{CPlusScratch}}"{Default}.impl.a -o {targDir}{Default}.impl.h.o ; Delete -i "{{CPlusScratch}}"{Default}.impl.a

# --- Other stuff -------------------------------------------------------------------

#BluntTool.pkg: RFCOMMService.o RFCOMMTool.o RelocHack.o RFCOMMService.impl.o EventsCommands.o
#	$(LD) $(LDFLAGS) -o BluntTool.bin $+ $(LIBS)
#	$(PACKER) -o $@ BluntTool $(PACKERFLAGS)  -protocol -aif BluntTool.bin -autoLoad -autoRemove
#	$(SETFILE) -t "pkg " -c "pkgX" $@

# {NCT-link} {NCT-link-options} -dupok -o :Objects:TRFCOMMToolMP2100D.bin ":{NCT-ObjectOut}:RelocHack.a.o" ":{NCT-ObjectOut}:THCILayer.cp.o" ":{NCT-ObjectOut}:TL2CAPLayer.cp.o" ":{NCT-ObjectOut}:TSDPLayer.cp.o" ":{NCT-ObjectOut}:TRFCOMMLayer.cp.o" ":{NCT-ObjectOut}:TRFCOMMTool.cp.o" ":{NCT-ObjectOut}:TRFCOMMService.cp.o" ":{NCT-ObjectOut}:TRFCOMMService.impl.h.o" "{NCT_Libraries}MP2100D.a.o"
# "{NCT-packer}" -o BluntMP2100D.pkg "Blunt" {NCT-packer-options} -packageid 'rfcm' -copyright 'Copyright (c) 2003 Eckhart Kšppen' -version 01 ¶
# -protocol -aif ":{NCT-ObjectOut}:TRFCOMMToolMP2100D.bin" -autoload -autoRemove
