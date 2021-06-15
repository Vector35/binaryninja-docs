# Copyright (c) 2015-2021 Vector 35 Inc
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

import ctypes
from typing import Generator, Optional, List, Tuple

# Binary Ninja components
import binaryninja
from . import _binaryninjacore as core
from .enums import BranchType, HighlightStandardColor
from .highlight import HighlightColor
from . import function as function_module
from . import binaryview
from . import architecture

class BasicBlockEdge(object):
	def __init__(self, branch_type:BranchType, source:'BasicBlock', target:'BasicBlock', back_edge:bool, fall_through:bool):
		self._type = branch_type
		self._source = source
		self._target = target
		self._back_edge = back_edge
		self._fall_through = fall_through

	def __repr__(self):
		if self._type == BranchType.UnresolvedBranch:
			return "<%s>" % BranchType(self._type).name
		elif self._target.arch:
			return "<%s: %s@%#x>" % (BranchType(self._type).name, self._target.arch.name, self._target.start)
		else:
			return "<%s: %#x>" % (BranchType(self._type).name, self._target.start)

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self._type, self._source, self._target, self._back_edge, self._fall_through) == \
			(other._type, other._source, other._target, other._back_edge, other._fall_through)

	def __ne__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return not (self == other)

	def __hash__(self):
		return hash((self._type, self._source, self._target, self.back_edge, self.fall_through))

	@property
	def type(self) -> BranchType:
		""" """
		return self._type

	@type.setter
	def type(self, value:BranchType) -> None:
		self._type = value

	@property
	def source(self) -> 'BasicBlock':
		""" """
		return self._source

	@source.setter
	def source(self, value:'BasicBlock') -> None:
		self._source = value

	@property
	def target(self) -> 'BasicBlock':
		""" """
		return self._target

	@target.setter
	def target(self, value:'BasicBlock') -> None:
		self._target = value

	@property
	def back_edge(self) -> bool:
		""" """
		return self._back_edge

	@back_edge.setter
	def back_edge(self, value:bool) -> None:
		self._back_edge = value

	@property
	def fall_through(self) -> bool:
		""" """
		return self._fall_through

	@fall_through.setter
	def fall_through(self, value:bool) -> None:
		self._fall_through = value



class BasicBlock(object):
	def __init__(self, handle:core.BNBasicBlock, view:Optional['binaryview.BinaryView']=None):
		self._view = view
		self.handle = core.handle_of_type(handle, core.BNBasicBlock)
		self._arch = None
		self._func = None
		self._instStarts:Optional[List[int]] = None
		self._instLengths:Optional[List[int]] = None

	def __del__(self):
		core.BNFreeBasicBlock(self.handle)

	def __repr__(self):
		arch = self.arch
		if arch:
			return "<block: %s@%#x-%#x>" % (arch.name, self.start, self.end)
		else:
			return "<block: %#x-%#x>" % (self.start, self.end)

	def __len__(self):
		return int(core.BNGetBasicBlockLength(self.handle))

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return ctypes.addressof(self.handle.contents) == ctypes.addressof(other.handle.contents)

	def __ne__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return not (self == other)

	def __hash__(self):
		return hash((self.start, self.end, self.arch.name))

	def __setattr__(self, name, value):
		try:
			object.__setattr__(self, name, value)
		except AttributeError:
			raise AttributeError("attribute '%s' is read only" % name)

	def __iter__(self) -> Generator[Tuple[List['function_module.InstructionTextToken'], int], None, None]:
		if self._instStarts is None:
			# don't add instruction start cache--the object is likely ephemeral
			idx = self.start
			while idx < self.end:
				data = self._view.read(idx, min(self.arch.max_instr_length, self.end - idx))
				inst_text = self.arch.get_instruction_text(data, idx)
				if inst_text[1] == 0:
					break
				yield inst_text
				idx += inst_text[1]
		else:
			assert self._instLengths is not None
			for start, length in zip(self._instStarts, self._instLengths):
				inst_text = self.arch.get_instruction_text(self._view.read(start, length), start)
				if inst_text[1] == 0:
					break
				yield inst_text

	def __getitem__(self, i):
		self._buildStartCache()
		start = self._instStarts[i]
		length = self._instLengths[i]
		data = self._view.read(start, length)
		return self.arch.get_instruction_text(data, start)

	def _buildStartCache(self) -> None:
		if self._instStarts is None:
			# build the instruction start cache
			self._instStarts = []
			self._instLengths = []
			start = self.start
			while start < self.end:
				length = self.view.get_instruction_length(start)
				if length == 0: # invalid instruction. avoid infinite loop
					break
				self._instLengths.append(length)
				self._instStarts.append(start)
				start += length

	def _create_instance(self, handle:core.BNBasicBlock, view:'binaryview.BinaryView') -> 'BasicBlock':
		"""Internal method used to instantiate child instances"""
		return BasicBlock(handle, view)

	@property
	def instruction_count(self) -> int:
		self._buildStartCache()
		assert self._instStarts is not None
		return len(self._instStarts)

	@property
	def function(self) -> Optional['function_module.Function']:
		"""Basic block function (read-only)"""
		if self._func is not None:
			return self._func
		func = core.BNGetBasicBlockFunction(self.handle)
		if func is None:
			return None
		self._func = function_module.Function(self._view, func)
		return self._func

	@property
	def view(self) -> 'binaryview.BinaryView':
		"""BinaryView that contains the basic block (read-only)"""
		if self._view is not None:
			return self._view
		self._view = self.function.view
		return self._view

	@property
	def arch(self) -> Optional['architecture.Architecture']:
		"""Basic block architecture (read-only)"""
		# The arch for a BasicBlock isn't going to change so just cache
		# it the first time we need it
		if self._arch is not None:
			return self._arch
		arch = core.BNGetBasicBlockArchitecture(self.handle)
		if arch is None:
			return None
		self._arch = architecture.CoreArchitecture._from_cache(arch)
		return self._arch

	@property
	def source_block(self) -> Optional['BasicBlock']:
		"""Basic block source block (read-only)"""
		block = core.BNGetBasicBlockSource(self.handle)
		if block is None:
			return None
		return BasicBlock(block, self._view)

	@property
	def start(self) -> int:
		"""Basic block start (read-only)"""
		return core.BNGetBasicBlockStart(self.handle)

	@property
	def end(self) -> int:
		"""Basic block end (read-only)"""
		return core.BNGetBasicBlockEnd(self.handle)

	@property
	def length(self) -> int:
		"""Basic block length (read-only)"""
		return core.BNGetBasicBlockLength(self.handle)

	@property
	def index(self) -> int:
		"""Basic block index in list of blocks for the function (read-only)"""
		return core.BNGetBasicBlockIndex(self.handle)

	@property
	def outgoing_edges(self) -> List[BasicBlockEdge]:
		"""List of basic block outgoing edges (read-only)"""
		count = ctypes.c_ulonglong(0)
		edges = core.BNGetBasicBlockOutgoingEdges(self.handle, count)
		result = []
		for i in range(0, count.value):
			branch_type = BranchType(edges[i].type)
			handle = core.BNNewBasicBlockReference(edges[i].target)
			assert handle is not None
			target = self._create_instance(handle, self.view)
			result.append(BasicBlockEdge(branch_type, self, target, edges[i].backEdge, edges[i].fallThrough))
		core.BNFreeBasicBlockEdgeList(edges, count.value)
		return result

	@property
	def incoming_edges(self) -> List[BasicBlockEdge]:
		"""List of basic block incoming edges (read-only)"""
		count = ctypes.c_ulonglong(0)
		edges = core.BNGetBasicBlockIncomingEdges(self.handle, count)
		result = []
		for i in range(0, count.value):
			branch_type = BranchType(edges[i].type)
			handle = core.BNNewBasicBlockReference(edges[i].target)
			assert handle is not None
			target = self._create_instance(handle, self.view)
			result.append(BasicBlockEdge(branch_type, target, self, edges[i].backEdge, edges[i].fallThrough))
		core.BNFreeBasicBlockEdgeList(edges, count.value)
		return result

	@property
	def has_undetermined_outgoing_edges(self) -> bool:
		"""Whether basic block has undetermined outgoing edges (read-only)"""
		return core.BNBasicBlockHasUndeterminedOutgoingEdges(self.handle)

	@property
	def can_exit(self) -> bool:
		"""Whether basic block can return or is tagged as 'No Return' (read-only)"""
		return core.BNBasicBlockCanExit(self.handle)

	@property
	def has_invalid_instructions(self) -> bool:
		"""Whether basic block has any invalid instructions (read-only)"""
		return core.BNBasicBlockHasInvalidInstructions(self.handle)

	@property
	def dominators(self) -> List['BasicBlock']:
		"""List of dominators for this basic block (read-only)"""
		count = ctypes.c_ulonglong()
		blocks = core.BNGetBasicBlockDominators(self.handle, count, False)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(blocks[i])
			assert handle is not None
			result.append(self._create_instance(handle, self.view))
		core.BNFreeBasicBlockList(blocks, count.value)
		return result

	@property
	def post_dominators(self) -> List['BasicBlock']:
		"""List of dominators for this basic block (read-only)"""
		count = ctypes.c_ulonglong()
		blocks = core.BNGetBasicBlockDominators(self.handle, count, True)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(blocks[i])
			assert handle is not None
			result.append(self._create_instance(handle, self.view))
		core.BNFreeBasicBlockList(blocks, count.value)
		return result

	@property
	def strict_dominators(self) -> List['BasicBlock']:
		"""List of strict dominators for this basic block (read-only)"""
		count = ctypes.c_ulonglong()
		blocks = core.BNGetBasicBlockStrictDominators(self.handle, count, False)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(blocks[i])
			assert handle is not None
			result.append(self._create_instance(handle, self.view))
		core.BNFreeBasicBlockList(blocks, count.value)
		return result

	@property
	def immediate_dominator(self) -> Optional['BasicBlock']:
		"""Immediate dominator of this basic block (read-only)"""
		result = core.BNGetBasicBlockImmediateDominator(self.handle, False)
		if not result:
			return None
		return self._create_instance(result, self.view)

	@property
	def immediate_post_dominator(self) -> Optional['BasicBlock']:
		"""Immediate dominator of this basic block (read-only)"""
		result = core.BNGetBasicBlockImmediateDominator(self.handle, True)
		if not result:
			return None
		return self._create_instance(result, self.view)

	@property
	def dominator_tree_children(self) -> List['BasicBlock']:
		"""List of child blocks in the dominator tree for this basic block (read-only)"""
		count = ctypes.c_ulonglong()
		blocks = core.BNGetBasicBlockDominatorTreeChildren(self.handle, count, False)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(blocks[i])
			assert handle is not None
			result.append(self._create_instance(handle, self.view))
		core.BNFreeBasicBlockList(blocks, count.value)
		return result

	@property
	def post_dominator_tree_children(self) -> List['BasicBlock']:
		"""List of child blocks in the post dominator tree for this basic block (read-only)"""
		count = ctypes.c_ulonglong()
		blocks = core.BNGetBasicBlockDominatorTreeChildren(self.handle, count, True)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(blocks[i])
			assert handle is not None
			result.append(self._create_instance(handle, self.view))
		core.BNFreeBasicBlockList(blocks, count.value)
		return result

	@property
	def dominance_frontier(self) -> List['BasicBlock']:
		"""Dominance frontier for this basic block (read-only)"""
		count = ctypes.c_ulonglong()
		blocks = core.BNGetBasicBlockDominanceFrontier(self.handle, count, False)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(blocks[i])
			assert handle is not None
			result.append(self._create_instance(handle, self.view))
		core.BNFreeBasicBlockList(blocks, count.value)
		return result

	@property
	def post_dominance_frontier(self) -> List['BasicBlock']:
		"""Post dominance frontier for this basic block (read-only)"""
		count = ctypes.c_ulonglong()
		blocks = core.BNGetBasicBlockDominanceFrontier(self.handle, count, True)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(blocks[i])
			assert handle is not None
			result.append(self._create_instance(handle, self.view))
		core.BNFreeBasicBlockList(blocks, count.value)
		return result

	@property
	def annotations(self) -> List[List['function_module.InstructionTextToken']]:
		"""List of automatic annotations for the start of this block (read-only)"""
		assert self.arch is not None, "attempting to get annotation from BasicBlock without architecture"
		return self.function.get_block_annotations(self.start, self.arch)

	@property
	def disassembly_text(self) -> List['function_module.DisassemblyTextLine']:
		"""
		``disassembly_text`` property which returns a list of function.DisassemblyTextLine objects for the current basic block.
		:Example:

			>>> current_basic_block.disassembly_text
			[<0x100000f30: _main:>, ...]
		"""
		return self.get_disassembly_text()

	@property
	def highlight(self) -> HighlightColor:
		"""Gets or sets the highlight color for basic block

		:Example:

			>>> current_basic_block.highlight = HighlightStandardColor.BlueHighlightColor
			>>> current_basic_block.highlight
			<color: blue>
		"""
		return HighlightColor._from_core_struct(core.BNGetBasicBlockHighlight(self.handle))

	@highlight.setter
	def highlight(self, value:HighlightColor) -> None:
		self.set_user_highlight(value)

	@property
	def is_il(self) -> bool:
		"""Whether the basic block contains IL"""
		return core.BNIsILBasicBlock(self.handle)

	@property
	def is_low_level_il(self) -> bool:
		"""Whether the basic block contains Low Level IL"""
		return core.BNIsLowLevelILBasicBlock(self.handle)

	@property
	def is_medium_level_il(self) -> bool:
		"""Whether the basic block contains Medium Level IL"""
		return core.BNIsMediumLevelILBasicBlock(self.handle)

	@classmethod
	def get_iterated_dominance_frontier(cls, blocks:List['BasicBlock']) -> List['BasicBlock']:
		if len(blocks) == 0:
			return []
		block_set = (ctypes.POINTER(core.BNBasicBlock) * len(blocks))() #type: ignore
		for i in range(len(blocks)):
			block_set[i] = blocks[i].handle
		count = ctypes.c_ulonglong()
		out_blocks = core.BNGetBasicBlockIteratedDominanceFrontier(block_set, len(blocks), count)
		result = []
		for i in range(0, count.value):
			handle = core.BNNewBasicBlockReference(out_blocks[i])
			assert handle is not None
			result.append(BasicBlock(handle, blocks[0].view))
		core.BNFreeBasicBlockList(out_blocks, count.value)
		return result

	def mark_recent_use(self) -> None:
		core.BNMarkBasicBlockAsRecentlyUsed(self.handle)

	def get_disassembly_text(self, settings:'function_module.DisassemblySettings'=None) -> List['function_module.DisassemblyTextLine']:
		"""
		``get_disassembly_text`` returns a list of DisassemblyTextLine objects for the current basic block.

		:param DisassemblySettings settings: (optional) DisassemblySettings object
		:Example:

			>>> current_basic_block.get_disassembly_text()
			[<0x100000f30: _main:>, <0x100000f30: push    rbp>, ... ]
		"""
		settings_obj = None
		if settings:
			settings_obj = settings.handle

		count = ctypes.c_ulonglong()
		lines = core.BNGetBasicBlockDisassemblyText(self.handle, settings_obj, count)
		result = []
		for i in range(0, count.value):
			addr = lines[i].addr
			if (lines[i].instrIndex != 0xffffffffffffffff) and hasattr(self, 'il_function'):
				il_instr = self.il_function[lines[i].instrIndex] # type: ignore
			else:
				il_instr = None
			color = HighlightColor._from_core_struct(lines[i].highlight)
			tokens = function_module.InstructionTextToken._from_core_struct(lines[i].tokens, lines[i].count)
			result.append(function_module.DisassemblyTextLine(tokens, addr, il_instr, color))
		core.BNFreeDisassemblyTextLines(lines, count.value)
		return result

	def set_auto_highlight(self, color:HighlightColor) -> None:
		"""
		``set_auto_highlight`` highlights the current BasicBlock with the supplied color.

		.. warning:: Use only in analysis plugins. Do not use in regular plugins, as colors won't be saved to the database.

		:param HighlightStandardColor or HighlightColor color: Color value to use for highlighting
		"""
		if not isinstance(color, HighlightStandardColor) and not isinstance(color, HighlightColor):
			raise ValueError("Specified color is not one of HighlightStandardColor, HighlightColor")
		if isinstance(color, HighlightStandardColor):
			color = HighlightColor(color)
		core.BNSetAutoBasicBlockHighlight(self.handle, color._get_core_struct())

	def set_user_highlight(self, color:HighlightColor) -> None:
		"""
		``set_user_highlight`` highlights the current BasicBlock with the supplied color

		:param HighlightStandardColor or HighlightColor color: Color value to use for highlighting
		:Example:

			>>> current_basic_block.set_user_highlight(HighlightColor(red=0xff, blue=0xff, green=0))
			>>> current_basic_block.set_user_highlight(HighlightStandardColor.BlueHighlightColor)
		"""
		if not isinstance(color, HighlightStandardColor) and not isinstance(color, HighlightColor):
			raise ValueError("Specified color is not one of HighlightStandardColor, HighlightColor")
		if isinstance(color, HighlightStandardColor):
			color = HighlightColor(color)
		core.BNSetUserBasicBlockHighlight(self.handle, color._get_core_struct())

	def get_instruction_containing_address(self, addr:int) -> Tuple[bool, int]:
		start = ctypes.c_uint64()
		ret:bool = core.BNGetBasicBlockInstructionContainingAddress(self.handle, addr, start)
		return ret, start.value
