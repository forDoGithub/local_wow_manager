module;
#include "framework.h"
export module mmap;

import memory;

export class mm {
private:
	bool fix_relocations(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header, std::size_t image_delta);
	bool resolve_imports(HANDLE process, DWORD pid, char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header, int print_pad);
	bool write_buffer_to_target_process(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header);
	bool call_tls_callbacks(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header, std::size_t image_delta);
	bool call_dll_main(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header);
public:
	mem memory;
	mm(HANDLE process, DWORD pid) : memory(process, pid) {}

	std::pair<std::uintptr_t, std::string> map_dll(const wchar_t* dll, int print_pad = 0);
};

std::pair<std::uintptr_t, std::string> mm::map_dll(const wchar_t* dll, int print_pad) {
	std::ifstream file(dll, std::ios_base::binary);
	if (!file.is_open())
		return { 0, "Invalid dll path" };

	if (!memory.validate_handle())
		return { 0, "Failed to get handle to process" };

	file.seekg(0, file.end);
	std::size_t file_size = file.tellg();
	file.seekg(0, file.beg);

	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(file_size);
	file.read(buffer.get(), file_size);

	IMAGE_DOS_HEADER* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer.get());
	IMAGE_NT_HEADERS* pe_header = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.get() + dos_header->e_lfanew);

	std::uintptr_t base = memory.virtual_alloc_ex(NULL, pe_header->OptionalHeader.SizeOfImage, MEM_RESERVE, PAGE_READWRITE);
	if (!base)
		return { 0, "Failed to allocate memory for DLL. Error: " + std::to_string(GetLastError()) };

	std::string print_padding(print_pad, ' ');
	std::printf("\n%sMapping: %ls \n", print_padding.c_str(), dll);
	std::printf("%s[+] Dll Base: 0x%llx \n", print_padding.c_str(), base);

	try {
		std::unique_ptr<char[]> image = std::make_unique<char[]>(pe_header->OptionalHeader.SizeOfImage);
		std::size_t image_delta = base - pe_header->OptionalHeader.ImageBase;
		IMAGE_SECTION_HEADER* section_header = IMAGE_FIRST_SECTION(pe_header);

		for (std::size_t i = 0; i < pe_header->FileHeader.NumberOfSections; i++) {
			std::uintptr_t section_rva = section_header[i].VirtualAddress;
			std::size_t section_size_disk = section_header[i].SizeOfRawData;
			std::size_t section_size_memory = section_header[i].Misc.VirtualSize;

			std::uintptr_t dest = memory.virtual_alloc_ex(base + section_rva, section_size_memory, MEM_COMMIT, PAGE_READWRITE);
			if (!dest)
				throw std::runtime_error("Failed to allocate memory for section. Error: " + std::to_string(GetLastError()));

			std::memcpy(image.get() + section_rva, buffer.get() + section_header[i].PointerToRawData, section_size_disk);
		}

		if (!this->fix_relocations(image.get(), base, pe_header, section_header, image_delta))
			throw std::runtime_error("Failed to fix relocations");
		std::printf("%s[+] Fixed relocations \n", print_padding.c_str());

		if (!this->resolve_imports(memory.process, memory.pid, image.get(), base, pe_header, section_header, print_pad))
			throw std::runtime_error("Failed to resolve imports");
		std::printf("%s[+] Fixed imports \n", print_padding.c_str());

		if (!this->write_buffer_to_target_process(image.get(), base, pe_header, section_header))
			throw std::runtime_error("Failed to write buffer to target process");
		std::printf("%s[+] Dll written into target process memory \n", print_padding.c_str());

		if (!this->call_tls_callbacks(image.get(), base, pe_header, section_header, image_delta))
			throw std::runtime_error("Failed to call TLS callbacks");
		std::printf("%s[+] Called DLL_PROCESS_ATTACH TLS callbacks (if there were any)\n", print_padding.c_str());

		if (!this->call_dll_main(image.get(), base, pe_header, section_header))
			throw std::runtime_error("Failed to call DllMain");
		std::printf("%s[+] Called Dll Main into target process \n\n", print_padding.c_str());

		return { base, "" };
	}
	catch (const std::exception& e) {
		this->memory.virtual_free_ex(base, MEM_RELEASE);
		return { 0, e.what() };
	}
	catch (...) {
		this->memory.virtual_free_ex(base, MEM_RELEASE);
		return { 0, "Unexpected error occurred, make sure the dll is valid/not corrupt" };
	}
}
bool mm::fix_relocations(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header, std::size_t image_delta) {
	IMAGE_DATA_DIRECTORY base_reloc_dir = pe_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	std::size_t offset_to_block_start = 0;

	while (offset_to_block_start < base_reloc_dir.Size) {
		IMAGE_BASE_RELOCATION* base_relocation = reinterpret_cast<IMAGE_BASE_RELOCATION*>(image + base_reloc_dir.VirtualAddress + offset_to_block_start);
		std::size_t num_of_entries = (base_relocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD); //num of entries in the reloc block

		void* block = image + base_reloc_dir.VirtualAddress + offset_to_block_start + sizeof(IMAGE_BASE_RELOCATION);
		std::uintptr_t page = reinterpret_cast<std::uintptr_t>(image + base_relocation->VirtualAddress); //page where the fixing is needed

		WORD* entry = reinterpret_cast<WORD*>(block);
		for (std::size_t j = 0; j < num_of_entries; j++) {
			BYTE relocation_type = entry[j] >> 12;
			WORD offset = entry[j] & 0xFFF;

			if (relocation_type == IMAGE_REL_BASED_ABSOLUTE)
				continue; //padding, we skip
			else if (relocation_type == IMAGE_REL_BASED_HIGHLOW)
				*reinterpret_cast<uint32_t*>(page + offset) += (image_delta & UINT32_MAX); //note: the mask here is probably useless since it should only be used for 32bit apps (although I haven't seen it written so I'll leave it like this)
			else if (relocation_type == IMAGE_REL_BASED_DIR64)
				*reinterpret_cast<uint64_t*>(page + offset) += image_delta; //this should be the only relocation type we should get (for x64 dlls)
			else
				throw std::runtime_error("Unsupported relocation type");
		}

		offset_to_block_start += base_relocation->SizeOfBlock;
	}

	return true;
}

bool mm::resolve_imports(HANDLE process, DWORD pid, char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header, int print_pad) {
	IMAGE_DATA_DIRECTORY import_dir = pe_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	IMAGE_DATA_DIRECTORY bound_image_dir = pe_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT];

	std::uintptr_t import_desc_start_address = reinterpret_cast<std::uintptr_t>(image) + import_dir.VirtualAddress;
	IMAGE_IMPORT_DESCRIPTOR* import_descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(import_desc_start_address);

	while (import_dir.Size > sizeof(IMAGE_IMPORT_DESCRIPTOR) + (reinterpret_cast<std::uintptr_t>(import_descriptor) - import_desc_start_address)) {
		LPCSTR module_name_ptr = reinterpret_cast<LPCSTR>(image + import_descriptor->Name);
		std::string module_name(module_name_ptr);

		if (module_name.find(VIRTUAL_DLL_PREFIX) != std::string::npos) {
			module_name = Utils::dll_from_map(module_name);  // Virtual DLL -> Get real DLL
			module_name_ptr = module_name.c_str();
		}

		std::wstring module_name_wide = std::wstring(module_name.begin(), module_name.end());
		HMODULE fn_module_target = memory.get_module(module_name_wide).hModule;
		HMODULE fn_module_local = LoadLibraryA(module_name_ptr);

		if (!fn_module_target) {
			std::unique_ptr<TCHAR[]> dependency_file_path = std::make_unique<TCHAR[]>(MAX_PATH);
			if (!GetModuleFileName(fn_module_local, dependency_file_path.get(), MAX_PATH))
				throw std::runtime_error("Failed to get import dependency path for " + module_name);

			auto [injected_base, error] = this->map_dll(dependency_file_path.get(), print_pad + 3);
			if (!error.empty()) {
				throw std::runtime_error("Failed to inject dependency DLL: " + error);
			}
			fn_module_target = reinterpret_cast<HMODULE>(injected_base);
		}

		if (!fn_module_target || !fn_module_local)
			throw std::runtime_error("Failed to get import dependency " + module_name + " base address");

		std::size_t fn_module_delta = reinterpret_cast<std::size_t>(fn_module_target) - reinterpret_cast<std::size_t>(fn_module_local);

		IMAGE_THUNK_DATA* import_lookup_table = reinterpret_cast<IMAGE_THUNK_DATA*>(image + import_descriptor->OriginalFirstThunk);
		IMAGE_THUNK_DATA* import_address_table = reinterpret_cast<IMAGE_THUNK_DATA*>(image + import_descriptor->FirstThunk);

		std::uintptr_t* ilt_entry = reinterpret_cast<std::uintptr_t*>(import_lookup_table);
		std::uintptr_t* iat_entry = reinterpret_cast<std::uintptr_t*>(import_address_table);

		while (*ilt_entry != 0) {
			std::uintptr_t fn_address_local;
			if (*ilt_entry & IMAGE_ORDINAL_FLAG64) { // Import by ordinal
				std::uintptr_t fn_ordinal = IMAGE_ORDINAL64(*ilt_entry);
				fn_address_local = reinterpret_cast<std::uintptr_t>(GetProcAddress(fn_module_local, MAKEINTRESOURCEA(fn_ordinal)));
			}
			else { // Import by name
				IMAGE_IMPORT_BY_NAME* import_info = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(image + *ilt_entry);
				LPCSTR fn_name = reinterpret_cast<LPCSTR>(&import_info->Name);
				fn_address_local = reinterpret_cast<std::uintptr_t>(GetProcAddress(fn_module_local, fn_name));
			}

			*iat_entry = fn_address_local + fn_module_delta;

			ilt_entry++;
			iat_entry++;
		}

		if (import_descriptor->TimeDateStamp == -1) {
			IMAGE_BOUND_IMPORT_DESCRIPTOR* image_bound_descriptor = reinterpret_cast<IMAGE_BOUND_IMPORT_DESCRIPTOR*>(image + bound_image_dir.VirtualAddress);
			import_descriptor->TimeDateStamp = image_bound_descriptor->TimeDateStamp;
		}
		else {
			import_descriptor->TimeDateStamp = pe_header->FileHeader.TimeDateStamp; // Dummy
		}

		import_descriptor++;
	}

	return true;
}


bool mm::write_buffer_to_target_process(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header) {
	//writing image buffer into target memory
	for (std::size_t i = 0; i < pe_header->FileHeader.NumberOfSections; i++) {
		if (!lstrcmpA(reinterpret_cast<LPCSTR>(section_header[i].Name), ".reloc"))
			continue; //doens't have a purpose once loaded into memory, deleting it will make it more troublesome for someone to get the dll from memory

		std::uintptr_t section_rva = section_header[i].VirtualAddress;
		std::size_t section_size = section_header[i].SizeOfRawData;
		if (!memory.sz_write(base + section_rva, image + section_rva, section_size))
			throw std::runtime_error("Failed to write to process memory");

		//fix memory protections to correct flags
		DWORD protection;
		if (section_header[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
			if (section_header[i].Characteristics & IMAGE_SCN_MEM_WRITE)
				protection = PAGE_EXECUTE_READWRITE;
			else if (section_header[i].Characteristics & IMAGE_SCN_MEM_READ)
				protection = PAGE_EXECUTE_READ;
			else
				protection = PAGE_EXECUTE;
		}
		else {
			if (section_header[i].Characteristics & IMAGE_SCN_MEM_WRITE)
				protection = PAGE_READWRITE;
			else if (section_header[i].Characteristics & IMAGE_SCN_MEM_READ)
				protection = PAGE_READONLY;
			else if (section_header[i].Characteristics & IMAGE_SCN_MEM_NOT_CACHED)
				protection = PAGE_NOCACHE;
			else
				protection = PAGE_NOACCESS;
		}

		DWORD old_protection; //dummy variable, VirtualProtectEx fails if old_protection not passed
		if (!memory.virtual_protect_ex(base + section_rva, section_size, protection, &old_protection))
			throw std::runtime_error("Failed to call VirtualProtect on target process");
	}

	return true;
}

//https://legend.octopuslabs.io/archives/2418/2418.htm
bool mm::call_tls_callbacks(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header, std::size_t image_delta) {
	//calling tls callbacks
	IMAGE_DATA_DIRECTORY tls_dir_entry = pe_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];

	/*	shellcode:
		movq rcx, <base addr of dll>
		movq rdx, 0x1
		movq r8, 0x0
		movq rax, <addr of callback>
		call rax
	*/

	if (tls_dir_entry.Size) {
		BYTE shellcode[] = { 0x48, 0xB9, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE, 0x48, 0xC7, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x49, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x48, 0xB8, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE, 0xFF, 0xD0 };
		std::memcpy(&shellcode[2], &base, sizeof(HMODULE));

		IMAGE_TLS_DIRECTORY* tls_dir = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(image + tls_dir_entry.VirtualAddress);
		std::uintptr_t* callbacks = reinterpret_cast<std::uintptr_t*>(image - pe_header->OptionalHeader.ImageBase + tls_dir->AddressOfCallBacks - image_delta); //ptr the the first elem of an array of callbacks (we subtract the image_delta to revert the changes from the reloc done before)

		while (*callbacks) {
			std::uintptr_t callback_addr_in_target = *callbacks; //note: the address of the callback already matches the one in memory since we did the relocations before
			std::memcpy(&shellcode[26], &callback_addr_in_target, sizeof(std::uintptr_t));
			if (!memory.execute_shellcode(shellcode, sizeof(shellcode)))
				throw std::runtime_error("Failed to call tls callbacks");

			callbacks++;
		}
	}
	return true;

}

bool mm::call_dll_main(char* image, std::uintptr_t base, IMAGE_NT_HEADERS* pe_header, IMAGE_SECTION_HEADER* section_header) {
	/* Shellcode:
		movq rcx, <HMODULE of dll> ; HMODULE of dll
		movq rdx, 0x1 ; fdwReason DLL_PROCESS_ATTACH
		movq r8, 0x0 ; lpvReserved 0 for DLL_PROCESS_ATTACH
		movq rax, <DLLMain>
		call rax
	*/

	BYTE shellcode[] = { 0x48, 0xB9, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE, 0x48, 0xC7, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x49, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x48, 0xB8, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE, 0xFF, 0xD0 };
	std::uintptr_t dll_entrypoint = base + pe_header->OptionalHeader.AddressOfEntryPoint;
	std::memcpy(&shellcode[2], &base, sizeof(HMODULE));
	std::memcpy(&shellcode[26], &dll_entrypoint, sizeof(std::uintptr_t));

	if (!memory.execute_shellcode(shellcode, sizeof(shellcode)))
		throw std::runtime_error("Failed to call dll entry point");

	return true;
}