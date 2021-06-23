# coding=utf-8
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
from typing import List, Generator, Optional, Union, Set, Mapping
from dataclasses import dataclass

import binaryninja
from . import _binaryninjacore as core
from . import decorators
from .enums import RegisterValueType, VariableSourceType, DeadStoreElimination

@dataclass(frozen=True)
class LookupTableEntry(object):
	from_values:List[int]
	to_value:int

	def __repr__(self):
		return f"[{', '.join([f'{i:#x}' for i in self.from_values])}] -> {self.to_value:#x}"

	def type(self):
		return RegisterValueType.LookupTableValue

@dataclass(frozen=True)
class Confidence:
	confidence:int=core.max_confidence

@dataclass(frozen=True)
class RegisterValue:
	value:int
	offset:int
	type:RegisterValueType = RegisterValueType.UndeterminedValue
	confidence:int=core.max_confidence

	def _to_api_object(self):
		result = core.BNRegisterValue()
		result.type = self.type
		result._value = self.value
		result.offset = self.offset
		return result

	def _to_api_object_with_confidence(self):
		result = core.BNRegisterValue()
		result.type = self.type
		result._value = self.value
		result.offset = self.offset
		result.confidence = self.confidence
		return result

	@classmethod
	def from_BNRegisterValue(cls, reg_value:Union[core.BNRegisterValue, core.BNRegisterValueWithConfidence],
		arch:Optional['binaryninja.architecture.Architecture']=None) -> 'RegisterValue':
		confidence = core.max_confidence
		if isinstance(reg_value, core.BNRegisterValueWithConfidence):
			confidence = reg_value.confidence
		if reg_value.state == RegisterValueType.EntryValue:
			reg = None
			if arch is not None:
				reg = arch.get_reg_name(binaryninja.architecture.RegisterIndex(reg_value.value))
			return EntryRegisterValue(reg_value.value, reg=reg, confidence=confidence)
		elif reg_value.state == RegisterValueType.ConstantValue:
			return ConstantRegisterValue(reg_value.value, confidence=confidence)
		elif reg_value.state == RegisterValueType.ConstantValue:
			return ConstantPointerRegisterValue(reg_value.value, confidence=confidence)
		elif reg_value.state == RegisterValueType.StackFrameOffset:
			return StackFrameOffsetRegisterValue(reg_value.value, reg_value.offset, confidence=confidence)
		elif reg_value.state == RegisterValueType.ImportedAddressValue:
			return ImportedAddressRegisterValue(reg_value.value, confidence=confidence)
		elif reg_value.state == RegisterValueType.UndeterminedValue:
			return Undetermined()
		assert False, f"RegisterValueType {reg_value.state} not handled"


@dataclass(frozen=True)
class Undetermined(RegisterValue):
	value:int = 0
	offset:int = 0
	type:RegisterValueType = RegisterValueType.UndeterminedValue


@dataclass(frozen=True)
class ConstantRegisterValue(RegisterValue):
	offset:int = 0
	type:RegisterValueType = RegisterValueType.ConstantValue

	def __repr__(self):
		return f"<const {self.value:#x}>"


@dataclass(frozen=True)
class ConstantPointerRegisterValue(RegisterValue):
	offset:int = 0
	type:RegisterValueType = RegisterValueType.ConstantPointerValue

	def __repr__(self):
		return f"<const ptr {self.value:#x}>"


@dataclass(frozen=True)
class ImportedAddressRegisterValue(RegisterValue):
	offset:int = 0
	type:RegisterValueType = RegisterValueType.ImportedAddressValue

	def __repr__(self):
		return f"<imported address from entry {self.value:#x}>"


@dataclass(frozen=True)
class ReturnAddressRegisterValue(RegisterValue):
	offset:int = 0
	type:RegisterValueType = RegisterValueType.ReturnAddressValue

	def __repr__(self):
		return "<return address>"


@dataclass(frozen=True)
class EntryRegisterValue(RegisterValue):
	value:int = 0
	offset:int = 0
	type:RegisterValueType = RegisterValueType.EntryValue
	reg:Optional['binaryninja.architecture.RegisterName'] = None

	def __repr__(self):
		return f"<entry {self.value}>"

	@classmethod
	def from_BNRegisterValue(cls, value:core.BNRegisterValue):
		cls(value.state, value.value, value.offset)


@dataclass(frozen=True)
class StackFrameOffsetRegisterValue(RegisterValue):
	type:RegisterValueType = RegisterValueType.StackFrameOffset

	def __repr__(self):
		return f"<stack frame offset {self.offset:#x}>"


@decorators.passive
class ValueRange(object):
	def __init__(self, start, end, step):
		self._start = start
		self._end = end
		self._step = step

	def __repr__(self):
		if self.step == 1:
			return f"<range: {self.start:#x} to {self.end:#x}>"
		return f"<range: {self.start:#x} to {self.end:#x}, step {self.step:#x}>"

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return self.start == other.start and self.end == other.end and self.step == other.step

	def __contains__(self, other):
		if not isinstance(other, int):
			return NotImplemented
		return other in range(self._start, self._end, self._step)

	@property
	def start(self):
		return self._start

	@property
	def end(self):
		return self._end

	@property
	def step(self):
		return self._step


@decorators.passive
class PossibleValueSet(object):
	"""
	`class PossibleValueSet` PossibleValueSet is used to define possible values
	that a variable can take. It contains methods to instantiate different
	value sets such as Constant, Signed/Unsigned Ranges, etc.
	"""
	def __init__(self, arch = None, value = None):
		if value is None:
			self._type = RegisterValueType.UndeterminedValue
			return
		self._type = RegisterValueType(value.state)
		if value.state == RegisterValueType.EntryValue:
			if arch is None:
				self._reg = value.value
			else:
				self._reg = arch.get_reg_name(value.value)
		elif value.state == RegisterValueType.ConstantValue:
			self._value = value.value
		elif value.state == RegisterValueType.ConstantPointerValue:
			self._value = value.value
		elif value.state == RegisterValueType.StackFrameOffset:
			self._offset = value.value
		elif value.state == RegisterValueType.SignedRangeValue:
			self._offset = value.value
			self._ranges = []
			for i in range(0, value.count):
				start = value.ranges[i].start
				end = value.ranges[i].end
				step = value.ranges[i].step
				if start & (1 << 63):
					start |= ~((1 << 63) - 1)
				if end & (1 << 63):
					end |= ~((1 << 63) - 1)
				self._ranges.append(ValueRange(start, end, step))
		elif value.state == RegisterValueType.UnsignedRangeValue:
			self._offset = value.value
			self._ranges = []
			for i in range(0, value.count):
				start = value.ranges[i].start
				end = value.ranges[i].end
				step = value.ranges[i].step
				self._ranges.append(ValueRange(start, end, step))
		elif value.state == RegisterValueType.LookupTableValue:
			self._table = []
			self._mapping = {}
			for i in range(0, value.count):
				from_list = []
				for j in range(0, value.table[i].fromCount):
					from_list.append(value.table[i].fromValues[j])
					self._mapping[value.table[i].fromValues[j]] = value.table[i].toValue
				self._table.append(LookupTableEntry(from_list, value.table[i].toValue))
		elif (value.state == RegisterValueType.InSetOfValues) or (value.state == RegisterValueType.NotInSetOfValues):
			self._values = set()
			for i in range(0, value.count):
				self._values.add(value.valueSet[i])
		self._count = value.count

	def __repr__(self):
		if self._type == RegisterValueType.EntryValue:
			return f"<entry {self.reg}>"
		if self._type == RegisterValueType.ConstantValue:
			return f"<const {self.value:#x}>"
		if self._type == RegisterValueType.ConstantPointerValue:
			return f"<const ptr {self.value:#x}>"
		if self._type == RegisterValueType.StackFrameOffset:
			return f"<stack frame offset {self._offset:#x}>"
		if self._type == RegisterValueType.SignedRangeValue:
			return f"<signed ranges: {repr(self.ranges)}>"
		if self._type == RegisterValueType.UnsignedRangeValue:
			return f"<unsigned ranges: {repr(self.ranges)}>"
		if self._type == RegisterValueType.LookupTableValue:
			return f"<table: {', '.join([repr(i) for i in self.table])}>"
		if self._type == RegisterValueType.InSetOfValues:
			return f"<in set([{', '.join(hex(i) for i in sorted(self.values))}])>"
		if self._type == RegisterValueType.NotInSetOfValues:
			return f"<not in set([{', '.join(hex(i) for i in sorted(self.values))}])>"
		if self._type == RegisterValueType.ReturnAddressValue:
			return "<return address>"
		return "<undetermined>"

	def __contains__(self, other):
		if self.type in [RegisterValueType.ConstantValue, RegisterValueType.ConstantPointerValue] and isinstance(other, int):
			return self.value == other
		if self.type in [RegisterValueType.ConstantValue, RegisterValueType.ConstantPointerValue] and hasattr(other, "value"):
			return self.value == other.value
		if not isinstance(other, int):
			return NotImplemented
		#Initial implementation only checks numbers, no set logic
		if self.type == RegisterValueType.StackFrameOffset:
			return NotImplemented
		if self.type in [RegisterValueType.SignedRangeValue, RegisterValueType.UnsignedRangeValue]:
			for rng in self.ranges:
				if other in rng:
					return True
			return False
		if self.type == RegisterValueType.InSetOfValues:
			return other in self.values
		if self.type == RegisterValueType.NotInSetOfValues:
			return not other in self.values
		return NotImplemented

	def __eq__(self, other):
		if self.type in [RegisterValueType.ConstantValue, RegisterValueType.ConstantPointerValue] and isinstance(other, int):
			return self.value == other
		if not isinstance(other, self.__class__):
			return NotImplemented
		if self.type in [RegisterValueType.ConstantValue, RegisterValueType.ConstantPointerValue]:
			return self.value == other.value
		elif self.type == RegisterValueType.StackFrameOffset:
			return self.offset == other.offset
		elif self.type in [RegisterValueType.SignedRangeValue, RegisterValueType.UnsignedRangeValue]:
			return self.ranges == other.ranges
		elif self.type in [RegisterValueType.InSetOfValues, RegisterValueType.NotInSetOfValues]:
			return self.values == other.values
		elif self.type == RegisterValueType.UndeterminedValue and hasattr(other, 'type'):
			return self.type == other.type
		else:
			return self == other

	def __ne__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return not (self == other)

	def _to_api_object(self):
		result = core.BNPossibleValueSet()
		result.state = RegisterValueType(self.type)
		if self.type == RegisterValueType.UndeterminedValue:
			return result
		elif self.type == RegisterValueType.ConstantValue:
			result.value = self.value
		elif self.type == RegisterValueType.ConstantPointerValue:
			result.value = self.value
		elif self.type == RegisterValueType.StackFrameOffset:
			result.offset = self.value
		elif self.type == RegisterValueType.SignedRangeValue:
			result.offset = self.value
			result.ranges = (core.BNValueRange * self.count)()
			for i in range(0, self.count):
				start = self.ranges[i].start
				end = self.ranges[i].end
				if start & (1 << 63):
					start |= ~((1 << 63) - 1)
				if end & (1 << 63):
					end |= ~((1 << 63) - 1)
				value_range = core.BNValueRange()
				value_range.start = start
				value_range.end = end
				value_range.step = self.ranges[i].step
				result.ranges[i] = value_range
			result.count = self.count
		elif self.type == RegisterValueType.UnsignedRangeValue:
			result.offset = self.value
			result.ranges = (core.BNValueRange * self.count)()
			for i in range(0, self.count):
				value_range = core.BNValueRange()
				value_range.start = self.ranges[i].start
				value_range.end = self.ranges[i].end
				value_range.step = self.ranges[i].step
				result.ranges[i] = value_range
			result.count = self.count
		elif self.type == RegisterValueType.LookupTableValue:
			result.table = []
			result.mapping = {}
			for i in range(self.count):
				from_list = []
				for j in range(0, len(self.table[i].from_values)):
					from_list.append(self.table[i].from_values[j])
					result.mapping[self.table[i].from_values[j]] = result.table[i].to_value
				result.table.append(LookupTableEntry(from_list, result.table[i].to_value))
			result.count = self.count
		elif (self.type == RegisterValueType.InSetOfValues) or (self.type == RegisterValueType.NotInSetOfValues):
			values = (ctypes.c_longlong * self.count)()
			i = 0
			for value in self.values:
				values[i] = value
				i += 1
			result.valueSet = ctypes.cast(values, ctypes.POINTER(ctypes.c_longlong))
			result.count = self.count
		return result

	@property
	def type(self) -> RegisterValueType:
		return self._type

	@property
	def reg(self) -> 'binaryninja.architecture.RegisterName':
		return self._reg

	@property
	def value(self) -> int:
		return self._value

	@property
	def offset(self) -> int:
		return self._offset

	@property
	def ranges(self) -> List[ValueRange]:
		return self._ranges

	@property
	def table(self) -> List[LookupTableEntry]:
		return self._table

	@property
	def mapping(self) -> Mapping[int, int]:
		return self._mapping

	@property
	def values(self) -> Set[int]:
		return self._values

	@property
	def count(self) -> int:
		return self._count

	@staticmethod
	def undetermined() -> 'PossibleValueSet':
		"""
		Create a PossibleValueSet object of type UndeterminedValue.

		:return: PossibleValueSet object of type UndeterminedValue
		:rtype: PossibleValueSet
		"""
		return PossibleValueSet()

	@staticmethod
	def constant(value:int) -> 'PossibleValueSet':
		"""
		Create a constant valued PossibleValueSet object.

		:param int value: Integer value of the constant
		:rtype: PossibleValueSet
		"""
		result = PossibleValueSet()
		result._type = RegisterValueType.ConstantValue
		result._value = value
		return result

	@staticmethod
	def constant_ptr(value:int) -> 'PossibleValueSet':
		"""
		Create constant pointer valued PossibleValueSet object.

		:param int value: Integer value of the constant pointer
		:rtype: PossibleValueSet
		"""
		result = PossibleValueSet()
		result._type = RegisterValueType.ConstantPointerValue
		result._value = value
		return result

	@staticmethod
	def stack_frame_offset(offset:int) -> 'PossibleValueSet':
		"""
		Create a PossibleValueSet object for a stack frame offset.

		:param int value: Integer value of the offset
		:rtype: PossibleValueSet
		"""
		result = PossibleValueSet()
		result._type = RegisterValueType.StackFrameOffset
		result._offset = offset
		return result

	@staticmethod
	def signed_range_value(ranges:List[ValueRange]) -> 'PossibleValueSet':
		"""
		Create a PossibleValueSet object for a signed range of values.

		:param list(ValueRange) ranges: List of ValueRanges
		:rtype: PossibleValueSet
		:Example:

			>>> v_1 = ValueRange(-5, -1, 1)
			>>> v_2 = ValueRange(7, 10, 1)
			>>> val = PossibleValueSet.signed_range_value([v_1, v_2])
			<signed ranges: [<range: -0x5 to -0x1>, <range: 0x7 to 0xa>]>
		"""
		result = PossibleValueSet()
		result._value = 0
		result._type = RegisterValueType.SignedRangeValue
		result._ranges = ranges
		result._count = len(ranges)
		return result

	@staticmethod
	def unsigned_range_value(ranges:List[ValueRange]) -> 'PossibleValueSet':
		"""
		Create a PossibleValueSet object for a unsigned signed range of values.

		:param list(ValueRange) ranges: List of ValueRanges
		:rtype: PossibleValueSet
		:Example:

			>>> v_1 = ValueRange(0, 5, 1)
			>>> v_2 = ValueRange(7, 10, 1)
			>>> val = PossibleValueSet.unsigned_range_value([v_1, v_2])
			<unsigned ranges: [<range: 0x0 to 0x5>, <range: 0x7 to 0xa>]>
		"""
		result = PossibleValueSet()
		result._value = 0
		result._type = RegisterValueType.UnsignedRangeValue
		result._ranges = ranges
		result._count = len(ranges)
		return result

	@staticmethod
	def in_set_of_values(values:Union[List[int], Set[int]]) -> 'PossibleValueSet':
		"""
		Create a PossibleValueSet object for a value in a set of values.

		:param list(int) values: List of integer values
		:rtype: PossibleValueSet
		"""
		result = PossibleValueSet()
		result._type = RegisterValueType.InSetOfValues
		result._values = set(values)
		result._count = len(values)
		return result

	@staticmethod
	def not_in_set_of_values(values) -> 'PossibleValueSet':
		"""
		Create a PossibleValueSet object for a value NOT in a set of values.

		:param list(int) values: List of integer values
		:rtype: PossibleValueSet
		"""
		result = PossibleValueSet()
		result._type = RegisterValueType.NotInSetOfValues
		result._values = set(values)
		result._count = len(values)
		return result

	@staticmethod
	def lookup_table_value(lookup_table, mapping) -> 'PossibleValueSet':
		"""
		Create a PossibleValueSet object for a value which is a member of a
		lookuptable.

		:param list(LookupTableEntry) lookup_table: List of table entries
		:param dict of (int, int) mapping: Mapping used for resolution
		:rtype: PossibleValueSet
		"""
		result = PossibleValueSet()
		result._type = RegisterValueType.LookupTableValue
		result._table = lookup_table
		result._mapping = mapping
		return result


@decorators.passive
class StackVariableReference(object):
	def __init__(self, src_operand, t, name, var, ref_ofs, size):
		self._source_operand = src_operand
		self._type = t
		self._name = name
		self._var = var
		self._referenced_offset = ref_ofs
		self._size = size
		if self._source_operand == 0xffffffff:
			self._source_operand = None

	def __repr__(self):
		if self._source_operand is None:
			if self._referenced_offset != self._var.storage:
				return "<ref to %s%+#x>" % (self._name, self._referenced_offset - self._var.storage)
			return "<ref to %s>" % self._name
		if self._referenced_offset != self._var.storage:
			return "<operand %d ref to %s%+#x>" % (self._source_operand, self._name, self._var.storage)
		return "<operand %d ref to %s>" % (self._source_operand, self._name)

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self._source_operand, self._type, self._name, self._var, self._referenced_offset, self._size) == \
			(other._source_operand, other._type, other._name, other._var, other._referenced_offset, other._size)

	def __ne__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return not (self == other)

	def __hash__(self):
		return hash((self._source_operand, self._type, self._name, self._var, self._referenced_offset, self._size))

	@property
	def source_operand(self):
		return self._source_operand

	@property
	def type(self):
		return self._type

	@property
	def name(self):
		return self._name

	@property
	def var(self):
		return self._var

	@property
	def referenced_offset(self):
		return self._referenced_offset

	@property
	def size(self):
		return self._size


@decorators.passive
@dataclass(frozen=True, order=True)
class CoreVariable:
	_source_type:int
	index:int
	storage:int

	@property
	def identifier(self) -> int:
		return core.BNToVariableIdentifier(self.to_BNVariable())

	@property
	def source_type(self) -> VariableSourceType:
		return VariableSourceType(self._source_type)

	def to_BNVariable(self):
		v = core.BNVariable()
		v.type = self._source_type
		v.index = self.index
		v.storage = self.storage
		return v

	@classmethod
	def from_BNVariable(cls, var:core.BNVariable):
		return cls(var.type, var.index, var.storage)

	@classmethod
	def from_identifier(cls, identifier):
		var = core.BNFromVariableIdentifier(identifier)
		return cls(var.type, var.index, var.storage)

@decorators.passive
@dataclass(frozen=True, order=True)
class VariableNameAndType(CoreVariable):
	name:str
	type:'binaryninja.types.Type'

	@classmethod
	def from_identifier(cls, identifier, name, type):
		var = core.BNFromVariableIdentifier(identifier)
		return cls(name, type, var.type, var.index, var.storage)

	@classmethod
	def from_core_variable(cls, var, name, type):
		return cls(name, type, var.type, var.index, var.storage)


class Variable:
	def __init__(self, func:'binaryninja.function.Function', source_type:VariableSourceType, index:int, storage:int):
		self._function = func
		self._var = CoreVariable(source_type, index, storage)

	@classmethod
	def from_variable_name_and_type(cls, func:'binaryninja.function.Function', var:VariableNameAndType):
		return cls(func, VariableSourceType(var.type), var.index, var.storage)

	@classmethod
	def from_core_variable(cls, func:'binaryninja.function.Function', var:CoreVariable):
		return cls(func, var.source_type, var.index, var.storage)

	@classmethod
	def from_BNVariable(cls, func:'binaryninja.function.Function', var:core.BNVariable):
		return cls(func, var.type, var.index, var.storage)

	@classmethod
	def from_identifier(cls, func:'binaryninja.function.Function', identifier:int):
		var = core.BNFromVariableIdentifier(identifier)
		return cls(func, VariableSourceType(var.type), var.index, var.storage)

	def __repr__(self):
		return f"<var {self.type.get_string_before_name()} {self.name}{self.type.get_string_after_name()}>"

	def __str__(self):
		return self.name

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self.identifier, self._function) == (other.identifier, other._function)

	def __ne__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return not (self == other)

	def __lt__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self.identifier, self._function) < (other.identifier, other._function)

	def __gt__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self.identifier, self._function) > (other.identifier, other._function)

	def __le__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self.identifier, self._function) <= (other.identifier, other._function)

	def __ge__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self.identifier, self._function) >= (other.identifier, other._function)

	def __hash__(self):
		return hash((self.identifier))

	@property
	def source_type(self) -> VariableSourceType:
		return self._var.source_type

	@property
	def index(self) -> int:
		return self._var.index

	@property
	def storage(self) -> int:
		"""Stack offset for StackVariableSourceType, register index for RegisterVariableSourceType"""
		return self._var.storage

	@property
	def identifier(self) -> int:
		return self._var.identifier

	@property
	def core_var(self) -> CoreVariable:
		return self._var

	@property
	def var_name_and_type(self) -> VariableNameAndType:
		return VariableNameAndType.from_core_variable(self._var, self.name, self.type)

	@property
	def name(self):
		"""Name of the variable"""
		return core.BNGetVariableName(self._function.handle, self._var.to_BNVariable())

	@name.setter
	def name(self, name:Optional[str]) -> None:
		if name is None:
			name = ""
		self._function.create_user_var(self, self.type, name)

	@property
	def type(self) -> 'binaryninja.types.Type':
		var_type_conf = core.BNGetVariableType(self._function.handle, self._var.to_BNVariable())
		assert var_type_conf.type
		_type = binaryninja.types.Type(var_type_conf.type, self._function.platform, var_type_conf.confidence)
		return _type

	@type.setter
	def type(self, new_type:'binaryninja.types.Type') -> None:
		self._function.create_user_var(self, new_type, self.name)

	@property
	def dead_store_elimination(self):
		return DeadStoreElimination(core.BNGetFunctionVariableDeadStoreElimination(self._function.handle, self._var.to_BNVariable()))

	@dead_store_elimination.setter
	def dead_store_elimination(self, value):
		core.BNSetFunctionVariableDeadStoreElimination(self._function.handle, self._var.to_BNVariable(), value)

	def to_BNVariable(self):
		return self._var.to_BNVariable()

@decorators.passive
class ConstantReference(object):
	def __init__(self, val, size, ptr, intermediate):
		self._value = val
		self._size = size
		self._pointer = ptr
		self._intermediate = intermediate

	def __repr__(self):
		if self.pointer:
			return "<constant pointer %#x>" % self.value
		if self.size == 0:
			return "<constant %#x>" % self.value
		return "<constant %#x size %d>" % (self.value, self.size)

	@property
	def value(self):
		return self._value

	@property
	def size(self):
		return self._size

	@property
	def pointer(self):
		return self._pointer

	@property
	def intermediate(self):
		return self._intermediate


@decorators.passive
class UserVariableValueInfo(object):
	def __init__(self, var, def_site, value):
		self.var = var
		self.def_site = def_site
		self.value = value

	def __repr__(self):
		return "<user value for %s @ %s:%#x -> %s>" % (self.var, self.def_site.arch.name, self.def_site.addr, self.value)


@decorators.passive
class IndirectBranchInfo(object):
	def __init__(self, source_arch, source_addr, dest_arch, dest_addr, auto_defined):
		self.source_arch = source_arch
		self.source_addr = source_addr
		self.dest_arch = dest_arch
		self.dest_addr = dest_addr
		self.auto_defined = auto_defined

	def __repr__(self):
		return "<branch %s:%#x -> %s:%#x>" % (self.source_arch.name, self.source_addr, self.dest_arch.name, self.dest_addr)


@decorators.passive
class ParameterVariables(object):
	def __init__(self, var_list:List[Variable], confidence:int=core.max_confidence, func:Optional['binaryninja.function.Function']=None):
		self._vars = var_list
		self._confidence = confidence
		self._func = func

	def __repr__(self):
		return repr(self._vars)

	def __len__(self):
		return len(self._vars)

	def __iter__(self) -> Generator['Variable', None, None]:
		for var in self._vars:
			yield var

	def __getitem__(self, idx) -> 'Variable':
		return self._vars[idx]

	def __setitem__(self, idx:int, value:'Variable'):
		self._vars[idx] = value
		if self._func is not None:
			self._func.parameter_vars = self

	def with_confidence(self, confidence:int) -> 'ParameterVariables':
		return ParameterVariables(list(self._vars), confidence, self._func)

	@property
	def vars(self) -> List['Variable']:
		return self._vars

	@property
	def confidence(self) -> int:
		return self._confidence

	@property
	def function(self) -> Optional['binaryninja.function.Function']:
		return self._func


@decorators.passive
class AddressRange(object):
	def __init__(self, start:int, end:int):
		self._start = start
		self._end = end

	def __repr__(self):
		return "<%#x-%#x>" % (self._start, self._end)

	def __len__(self):
		return self._end - self.start

	def __eq__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return (self._start, self._end) == (other._start, other._end)

	def __ne__(self, other):
		if not isinstance(other, self.__class__):
			return NotImplemented
		return not (self == other)

	def __hash__(self):
		return hash((self._start, self._end))

	@property
	def length(self) -> int:
		return self._end - self._start

	@property
	def start(self) -> int:
		return self._start

	@property
	def end(self) -> int:
		return self._end