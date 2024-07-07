module;
#include "framework.h"
export module memory;

#define FLAG_HIJACK_THREAD_FINISHED 0x1
#define PAGE_READABLE PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
#define	IS_HANDLE_VALID(handle) (handle != nullptr && handle != INVALID_HANDLE_VALUE)

//TODO: implement array of memory addresses + sizes to cleanup on destruction (have a flag in the codecave function)
export class mem {
private:
	undocumented::ntdll::PEB peb = { 0 };
public:
	DWORD pid = 0;
	HANDLE process = INVALID_HANDLE_VALUE;
	mem(HANDLE process, DWORD pid) : process(process), pid(pid) { }

	bool validate_handle();
	std::uintptr_t get_image_base();
	MODULEENTRY32W get_module(const std::wstring& mod_name); //TODO: change implementation like done in hijack_thread
	std::uintptr_t find_codecave(std::uintptr_t start, std::uintptr_t end, std::size_t size, DWORD protection_flags);
	bool execute_shellcode(BYTE* shellcode, std::size_t shellcode_size); //function takes care of restoring flags and general purpose regsiters + restore codecave to original status

	//wrappers
	template <typename TYPE>
	TYPE read(std::uintptr_t address) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		TYPE out = { 0 };
		if (!ReadProcessMemory(this->process, reinterpret_cast<LPCVOID>(address), &out, sizeof(TYPE), nullptr))
			return {}; //read failed

		return out;
	}

	bool sz_read(std::uintptr_t address, void* out, std::size_t size) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		if (!ReadProcessMemory(this->process, reinterpret_cast<LPCVOID>(address), out, size, nullptr))
			return NULL; //read failed

		return true;
	}

	template <typename TYPE>
	bool write(std::uintptr_t dest, TYPE* data) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		return WriteProcessMemory(this->process, reinterpret_cast<LPVOID>(dest), data, sizeof(data), nullptr);
	}

	bool sz_write(std::uintptr_t dest, void* data, std::size_t size) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		return WriteProcessMemory(this->process, reinterpret_cast<LPVOID>(dest), data, size, nullptr);
	}

	std::uintptr_t virtual_alloc_ex(std::uintptr_t address, std::size_t size, DWORD alloc_type, DWORD protect) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		return reinterpret_cast<std::uintptr_t>(VirtualAllocEx(this->process, reinterpret_cast<LPVOID>(address), size, alloc_type, protect));
	}

	bool virtual_protect_ex(std::uintptr_t address, std::size_t size, DWORD new_protect, DWORD* old_protect) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		return VirtualProtectEx(this->process, reinterpret_cast<LPVOID>(address), size, new_protect, old_protect);
	}

#pragma warning(push)
#pragma warning (disable: 28160)
	bool virtual_free_ex(std::uintptr_t address, DWORD free_type, std::size_t size = 0) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		return VirtualFreeEx(this->process, reinterpret_cast<LPVOID>(address), size, free_type);
	}
#pragma warning(pop)

	bool query_information_process(PROCESSINFOCLASS proc_info_class, void* out, std::size_t out_size) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		NTSTATUS ret = NtQueryInformationProcess(this->process, proc_info_class, out, static_cast<ULONG>(out_size), nullptr);

		return NT_SUCCESS(ret);
	}

	bool zero_out_memory(std::uintptr_t start, std::size_t size) {
		if (!IS_HANDLE_VALID(this->process))
			throw std::runtime_error("No valid handle");

		std::unique_ptr<BYTE[]> zeros = std::make_unique<BYTE[]>(size);
		std::memset(zeros.get(), 0x00, size);
		return this->sz_write(start, zeros.get(), size);
	}
};

bool mem::validate_handle() {
	return IS_HANDLE_VALID(process);
}

std::uintptr_t mem::get_image_base() {
	if (!IS_HANDLE_VALID(this->process))
		throw std::runtime_error("No valid handle");

	if (this->peb.ImageBaseAddress != NULL)
		return reinterpret_cast<std::uintptr_t>(this->peb.ImageBaseAddress);

	PROCESS_BASIC_INFORMATION proc_basic_info = { 0 };
	if (!this->query_information_process(ProcessBasicInformation, &proc_basic_info, sizeof(PROCESS_BASIC_INFORMATION)))
		return 0;

	undocumented::ntdll::PEB target_proc_peb = { 0 };
	if (!this->sz_read(reinterpret_cast<std::uintptr_t>(proc_basic_info.PebBaseAddress), &target_proc_peb, sizeof(undocumented::ntdll::PEB)))
		return 0;

	this->peb = target_proc_peb;
	return reinterpret_cast<std::uintptr_t>(this->peb.ImageBaseAddress);
}

MODULEENTRY32W mem::get_module(const std::wstring& mod_name) {
	if (!IS_HANDLE_VALID(this->process))
		throw std::runtime_error("No valid handle");

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, this->pid);
	MODULEENTRY32 mod_entry = { 0 };
	mod_entry.dwSize = sizeof(MODULEENTRY32);
	if (snapshot != INVALID_HANDLE_VALUE) {
		if (Module32First(snapshot, &mod_entry)) {
			do {
				if (!_wcsicmp(mod_name.c_str(), mod_entry.szModule)) {
					CloseHandle(snapshot);
					return mod_entry;
				}
			} while (Module32Next(snapshot, &mod_entry));
		}
		CloseHandle(snapshot);
	}
	return { 0 };
}

#pragma warning(disable: 6385)
std::uintptr_t mem::find_codecave(std::uintptr_t start, std::uintptr_t end, std::size_t size, DWORD protection_flags) {
	if (!IS_HANDLE_VALID(this->process))
		throw std::runtime_error("No valid handle");

	MEMORY_BASIC_INFORMATION mem_info = { 0 };
	std::uintptr_t query_addr = start;

	while (query_addr < end) {
		std::size_t bytes_read = VirtualQueryEx(this->process, reinterpret_cast<LPCVOID>(query_addr), &mem_info, sizeof(mem_info));
		if (!bytes_read)
			return 0; //VirtualQuery failed (no more page to scan, ...)

		query_addr = reinterpret_cast<std::uintptr_t>(mem_info.BaseAddress); //need to set it to BaseAddress if one of the pages protection has been modified resulting in a split

		if (mem_info.Protect & protection_flags && !(mem_info.Protect & PAGE_GUARD) && mem_info.State == MEM_COMMIT) {
			std::unique_ptr<char[]> page = std::make_unique<char[]>(mem_info.RegionSize);
			if (!this->sz_read(query_addr, page.get(), mem_info.RegionSize)) {
				int temp = GetLastError();
				return 0; //failed to read memory //TODO: some error here, should prob not exit and copntinue
			}

			for (std::size_t i = mem_info.RegionSize - 1; i >= size - 1; i--) {
				for (std::size_t j = 0; j <= size; j++) {
					if (j == size)
						return query_addr + i - j + 1;;

					if (page[i - j] != 0x00)
						break;
				}
			}
		}
		query_addr += mem_info.RegionSize;
	}
	return 0; //no codecave found
}

bool mem::execute_shellcode(BYTE* shellcode, std::size_t shellcode_size) { //note: no need for cleanup in the shellcode passed in args
	if (!IS_HANDLE_VALID(this->process))
		throw std::runtime_error("No valid handle");

	/*
		start_restore: //TODO: store xmm regs IMPORTANT
		pushfq	; push flags on stack
		push rax ; push all caller saved registers
		push rdi
		push rsi
		push rdx
		push rcx
		push r8
		push r9
		push r10
		push r11

		push rsp ; align the stack to a multiple of 0x10 (C calling convention :))))
		push [rsp]
		and rsp, -0x10

		sub rsp, 0x20 ; calling convention, need to reverse this space in the stack for the function called
	*/

	BYTE start_restore_orig_state[] = { 0x9C, 0x50, 0x57, 0x56, 0x52, 0x51, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x54, 0xFF, 0x34, 0x24, 0x48, 0x83, 0xE4, 0xF0, 0x48, 0x83, 0xEC, 0x20 };

	/*
		end_restore:
			mov rax, <byte check addr>
			mov byte ptr [rax], 0x1 ;set byte to indicate our shellcode has finished

			add rsp, 0x20 ; restore the space reserved

			mov rsp, [rsp + 8] ; restore old stack alignment

			pop r11
			pop r10
			pop r9
			pop r8
			pop rcx
			pop rdx
			pop rsi
			pop rdi
			pop rax
			popfq

			jmp qword ptr [rip] ; resume to old control flow
			<old rip here> ; since we jump to rip, it will take this value for the jump. This way we can make an absolute jump //https://stackoverflow.com/questions/9815448/jmp-instruction-hex-code
	*/

	BYTE end_restore_orig_state[] = { 0x48, 0xB8, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE, 0xC6, 0x00, 0x01, 0x48, 0x83, 0xC4, 0x20, 0x48, 0x8B, 0x64, 0x24, 0x08, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x59, 0x5A, 0x5E, 0x5F, 0x58, 0x9D, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE };


	std::size_t complete_shellcode_size = sizeof(start_restore_orig_state) + shellcode_size + sizeof(end_restore_orig_state);

	//we'll first check for a codecave, if it doesn't exists we create a page ourself
	std::uintptr_t shellcode_address_target = this->find_codecave(0, UINT64_MAX, complete_shellcode_size, PAGE_EXECUTE_READWRITE);
	bool allocated = false;
	if (!shellcode_address_target) {
		shellcode_address_target = this->virtual_alloc_ex(NULL, complete_shellcode_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (!shellcode_address_target)
			return 0;
		else
			allocated = true;
	}

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, this->pid);
	if (!IS_HANDLE_VALID(snapshot))
		return false;

	THREADENTRY32 thread_entry = { 0 };
	thread_entry.dwSize = sizeof(THREADENTRY32);

	if (!Thread32First(snapshot, &thread_entry))
		return false;

	bool success = false;
	do {
		if (thread_entry.th32OwnerProcessID != this->pid)
			continue;

		HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, thread_entry.th32ThreadID);
		if (IS_HANDLE_VALID(thread)) {
			if (SuspendThread(thread) != (DWORD)-1) {
				CONTEXT context = { 0 };
				context.ContextFlags = CONTEXT_FULL;
				if (GetThreadContext(thread, &context)) {
					std::memcpy(&end_restore_orig_state[2], &shellcode_address_target, sizeof(std::uintptr_t)); //where the FLAG_HIJACK_THREAD_FINISHED will be set 
					std::memcpy(&end_restore_orig_state[42], &context.Rip, sizeof(std::uintptr_t)); //copy the old eip into 'restore' shellcode
					context.Rip = shellcode_address_target;

					std::unique_ptr<BYTE[]> complete_shellcode = std::make_unique<BYTE[]>(complete_shellcode_size);
					std::memcpy(complete_shellcode.get(), start_restore_orig_state, sizeof(start_restore_orig_state));
					std::memcpy(&complete_shellcode[sizeof(start_restore_orig_state)], shellcode, shellcode_size);
					std::memcpy(&complete_shellcode[sizeof(start_restore_orig_state) + shellcode_size], end_restore_orig_state, sizeof(end_restore_orig_state));
					if (this->sz_write(shellcode_address_target, complete_shellcode.get(), complete_shellcode_size)) {
						SetThreadContext(thread, &context);
						ResumeThread(thread);
						while (this->read<BYTE>(shellcode_address_target) != FLAG_HIJACK_THREAD_FINISHED)
							Utils::sleep(10);

						success = true;
					}
					else {
						ResumeThread(thread);
					}
				}
			}
			CloseHandle(thread);
		}
		if (success)
			break;
	} while (Thread32Next(snapshot, &thread_entry));

	if (!allocated)
		this->zero_out_memory(shellcode_address_target, complete_shellcode_size);
	else
		this->virtual_free_ex(shellcode_address_target, MEM_RELEASE);

	CloseHandle(snapshot);

	return success;
}