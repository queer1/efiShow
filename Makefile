ARCH            = ia32

OBJS            = efiShow.o
TARGET          = efiShow.efi

EFIINC          = /usr/include/efi
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = /usr/lib32
EFILIB          = /usr/lib32
EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
				  		  -fshort-wchar -mno-red-zone -Wall -m32
ifeq ($(ARCH),x86_64)
	  CFLAGS += -DEFI_FUNCTION_WRAPPER
  endif

LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared \
				  		  -Bsymbolic -L $(EFILIB) -L $(LIB) $(EFI_CRT_OBJS) 

all: $(TARGET)

efiShow.so: $(OBJS)
	ld $(LDFLAGS) $(OBJS) -o $@ -lefi -lgnuefi

%.efi: %.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
					-j .dynsym  -j .rel -j .rela -j .reloc \
							--target=efi-app-$(ARCH) $^ $@
