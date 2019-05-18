//Parses the patch table of a TempleOS binary file to create functions
//@author Alessandro Arzilli
//@category _NEW_
//@keybinding 
//@menupath 
//@toolbar 

import ghidra.app.script.GhidraScript;
import ghidra.program.model.util.*;
import ghidra.program.model.reloc.*;
import ghidra.program.model.data.*;
import ghidra.program.model.block.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.scalar.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.address.*;
import ghidra.program.util.DefinedDataIterator;

import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;

public class TempleOS extends GhidraScript {
	protected static final byte IET_END = 0;
	protected static final byte IET_REL_I0 = 2; //Fictitious
	protected static final byte IET_IMM_U0 = 3; //Fictitious
	protected static final byte IET_REL_I8 = 4;
	protected static final byte IET_IMM_U8 = 5;
	protected static final byte IET_REL_I16 = 6;
	protected static final byte IET_IMM_U16 = 7;
	protected static final byte IET_REL_I32 = 8;
	protected static final byte IET_IMM_U32 = 9;
	protected static final byte IET_REL_I64 = 10;
	protected static final byte IET_IMM_I64 = 11;
	protected static final byte IET_REL32_EXPORT = 16;
	protected static final byte IET_IMM32_EXPORT = 17;
	protected static final byte IET_REL64_EXPORT = 18; //Not implemented
	protected static final byte IET_IMM64_EXPORT = 19; //Not implemented
	protected static final byte IET_ABS_ADDR = 20;
	protected static final byte IET_CODE_HEAP = 21; //Not really used
	protected static final byte IET_ZEROED_CODE_HEAP = 22; //Not really used
	protected static final byte IET_DATA_HEAP = 23;
	protected static final byte IET_ZEROED_DATA_HEAP = 24; //Not really used
	protected static final byte IET_MAIN = 25;
	
	public static Map<Byte, String> relocTypeName = new HashMap<Byte, String>() {{
		put(IET_END, "IET_END");
		put(IET_REL_I0, "IET_REL_I0");
		put(IET_IMM_U0, "IET_IMM_U0");
		put(IET_REL_I8, "IET_REL_I8");
		put(IET_IMM_U8, "IET_IMM_U8");
		put(IET_REL_I16, "IET_REL_I16");
		put(IET_IMM_U16, "IET_IMM_U16");
		put(IET_REL_I32, "IET_REL_I32");
		put(IET_IMM_U32, "IET_IMM_U32");
		put(IET_REL_I64, "IET_REL_I64");
		put(IET_IMM_I64, "IET_IMM_I64");
		put(IET_REL32_EXPORT, "IET_REL32_EXPORT");
		put(IET_IMM32_EXPORT, "IET_IMM32_EXPORT");
		put(IET_REL64_EXPORT, "IET_REL64_EXPORT");
		put(IET_IMM64_EXPORT, "IET_IMM64_EXPORT");
		put(IET_ABS_ADDR, "IET_ABS_ADDR");
		put(IET_CODE_HEAP, "IET_CODE_HEAP");
		put(IET_ZEROED_CODE_HEAP, "IET_ZEROED_CODE_HEAP");
		put(IET_DATA_HEAP, "IET_DATA_HEAP");
		put(IET_ZEROED_DATA_HEAP, "IET_ZEROED_DATA_HEAP");
		put(IET_MAIN, "IET_MAIN");
	}};
	
	static class Reader {
		private final Memory mem;
		private Address addr;
		private final Address zero;
		
		public Reader(final Memory mem, final Address zero) {
			this.mem = mem;
			this.addr = zero;
			this.zero = zero;
		}
		
		public void seekCur(int n) {
			this.addr = this.addr.add(n);
		}
		
		public void seekStart(int off) {
			this.addr = this.zero.add(off);
		}
		
		public byte read8() throws MemoryAccessException {
			final byte r = mem.getByte(addr);
			seekCur(1);
			return r;
		}
		
		public short read16() throws MemoryAccessException {
			final short r = mem.getShort(addr);
			seekCur(2);
			return r;
		}
		
		public int read32() throws MemoryAccessException {
			final int r = mem.getInt(addr);
			seekCur(4);
			return r;
		}
		
		public long read64() throws MemoryAccessException {
			final long r = mem.getLong(addr);
			seekCur(8);
			return r;
		}
		
		public String readString() throws MemoryAccessException {
			final Address start = pos();
			for (;;) {
				if (read8() == 0) {
					break;
				}
			}
			final long sz = pos().subtract(start);
			final byte[] buf = new byte[(int)sz-1];
			mem.getBytes(start, buf);
			return new String(buf);
		}
		
		public Address pos() {
			return addr.add(0);
		}
	}
	
	protected static final boolean doCleanup = true;
	protected static final boolean debugRelocationReader = false;
	protected static final boolean createPatchTableReferences = false;
	
	public void scanPatchTable(final Reader rdr, final long patchTableOffset, final Data patchTableOffsetData, final Memory mem, final Address moduleBase, final int pass) throws Exception {
		printf("patch_table_offset %x (pass %d)\n", patchTableOffset, pass);
		rdr.seekStart((int)patchTableOffset);
		if (pass == 1) {
			createLabel(rdr.pos(), "TOSB_PATCH_TABLE", true);
			createMemoryReference(patchTableOffsetData, rdr.pos(), RefType.DATA);
		}
		
		String lastImportName = "";
		
		for (;;) {
			final Address start = rdr.pos();
			createByte(rdr.pos());
			final byte etype = rdr.read8();
			
			if (etype == IET_END) {
				break;
			}
			final Data argData = createDWord(rdr.pos());
			final int arg = rdr.read32();
			final String str = rdr.readString();
			
			if (debugRelocationReader) {
				printf("found relocation %d i=%04x <%s>\n", etype, arg, str);
			}
			
			final String relocName = relocTypeName.get(etype);
			
			switch (etype) {
			case IET_IMM32_EXPORT: // fallthrough
			case IET_IMM64_EXPORT:
				if (pass == 1) {
					setEOLComment(start, String.format("export entry %d %s arg=%04x <%s>", etype, relocName, arg, str));
				}
				break;
				
			case IET_REL64_EXPORT: // fallthrough
			case IET_REL32_EXPORT: {
				setEOLComment(start, String.format("export entry %d %s arg=%04x <%s>", etype, relocName, arg, str));
				if (pass == 1) {
					final Address addr = moduleBase.add(arg);
					createFunction(addr, str);
					disassemble(addr);
					createMemoryReference(argData, addr, RefType.DATA);
				}
				break;
			}
			
			case IET_REL_I0: // fallthrough
			case IET_IMM_U0: // fallthrough
			case IET_REL_I8: // fallthrough
			case IET_IMM_U8: // fallthrough
			case IET_REL_I16: // fallthrough
			case IET_IMM_U16: // fallthrough
			case IET_REL_I32: // fallthrough
			case IET_IMM_U32: // fallthrough
			case IET_REL_I64: // fallthrough
			case IET_IMM_I64: 
				if (pass == 2) {
					setEOLComment(start, String.format("import entry %d %s arg=%04x <%s>", etype, relocName, arg, str));
					if (str != "") {
						lastImportName = str;
					}
					final Instruction instr = getInstructionContaining(moduleBase.add(arg));
					if (instr != null) {
						if (createPatchTableReferences) {
							createMemoryReference(argData, moduleBase.add(arg), RefType.DATA);
						}
						setEOLComment(moduleBase.add(arg), lastImportName);
					} else {
						setEOLComment(start, "no instruction at specified destination");
					}
				}
				break;
			
			case IET_ABS_ADDR: {
				if (pass == 2) {
					setEOLComment(start, String.format("abs address entry %d %s arg=%04x <%s>", etype, relocName, arg, str));
				}
				final int cnt = arg;
				for (int j = 0; j < cnt; ++j) {
					final Address pos = rdr.pos();
					final Data ptd = createDWord(rdr.pos());
					final int off = rdr.read32();
					if (pass == 2) {
						final int a = mem.getInt(moduleBase.add(off));
						final Instruction instr = getInstructionContaining(moduleBase.add(off));
						if (instr != null) {
							if (createPatchTableReferences) {
								createMemoryReference(ptd, moduleBase.add(off), RefType.DATA);						
							}
							try {
								createMemoryReference(instr, -1, moduleBase.add(a), RefType.DATA);
							} catch (Exception e) {
								setEOLComment(pos, String.format("could not create reference at destination: %s", e));
							}
						} else {
							setEOLComment(pos, "no instruction at specified destination");
						}
					}
				}
				break;
			}
			
			case IET_CODE_HEAP: // fallthrough
			case IET_ZEROED_CODE_HEAP: {
				if (pass == 2) {
					setEOLComment(start, String.format("code heap entry %d %s arg=%04x <%s>", etype, relocName, arg, str));
				}
				final int cnt = arg;
				for (int j = 0; j < cnt; ++j) {
					rdr.read32();
				}
				break;
			}
			
			case IET_DATA_HEAP: // fallthrough
			case IET_ZEROED_DATA_HEAP: {
				if (pass == 2) {
					setEOLComment(start, String.format("data heap entry %d %s arg=%04x <%s>", etype, relocName, arg, str));
				}
				final int cnt = arg;
				for (int j = 0; j < cnt; ++j) {
					rdr.read64();
				}
				break;
			}
			
			case IET_MAIN:
				if (pass == 1) {
					setEOLComment(start, String.format("main entry %d %s arg=%04x <%s>", etype, relocName, arg, str));
					final Address addr = moduleBase.add(arg);
					createFunction(addr, "MAIN");
					disassemble(addr);
				}
				break;
			}
		}
	}
	
	public void run() throws Exception {
		final Program p = getCurrentProgram();
		final Memory mem = p.getMemory();
		final Address zero = p.getMinAddress();
		final Listing lst = p.getListing();
		
		if (doCleanup) {
			clearListing(zero, p.getMaxAddress());
			
			for (final Function f: p.getFunctionManager().getFunctions(false)) {
				removeFunction(f);
			}
			
			for (final Data d: DefinedDataIterator.byDataType(p, d -> true)) {
				removeData(d);
			}
			
			for (final Symbol sym: p.getSymbolTable().getSymbolIterator()) {
				sym.delete();
			}
			
			final Iterator<Bookmark> it = p.getBookmarkManager().getBookmarksIterator();
			while (it.hasNext()) {
				p.getBookmarkManager().removeBookmark(it.next());
			}
			
			if ((mem.getByte(zero.add(4)) != 'T') || (mem.getByte(zero.add(5)) != 'O') || (mem.getByte(zero.add(6)) != 'S') || (mem.getByte(zero.add(7)) != 'B')) {
				println("signature check failed");
				return;
			}
		}
		
		setEOLComment(zero.add(0x8), "TOSB Header: org");
		createQWord(zero.add(0x8));
		setEOLComment(zero.add(0x10), "TOSB Header: patch_table_offset");
		final Data patchTableOffsetData = createQWord(zero.add(0x10));
		setEOLComment(zero.add(0x18), "TOSB Header: file size");
		createQWord(zero.add(0x18));
		
		final Address moduleBase = zero.add(0x20);
		createLabel(moduleBase, "MODULE_BASE", true);
		
		final Reader rdr = new Reader(mem, zero);
		rdr.seekStart(0x10);
		final long patchTableOffset = rdr.read64();
		
		scanPatchTable(rdr, patchTableOffset, patchTableOffsetData, mem, moduleBase, 1);
		scanPatchTable(rdr, patchTableOffset, patchTableOffsetData, mem, moduleBase, 2);
		println("Done");
	}
}
