"""Structured parser for LCP diagnostic console reports."""

from __future__ import annotations

import dataclasses
import re

_SECTION_LINE = re.compile(r"^\s*\[\s*(.*?)\s*\]\s*$")
_GROUP_LINE = re.compile(r"^\s*--\s*(.*?)\s*--\s*$")
_ASSIGNMENT = re.compile(r"(?P<key>[A-Za-z_][A-Za-z0-9_.\[\]-]*)\s*=\s*")
_SUBGROUP_PREFIX = re.compile(
    r"^(?:S|ETH)\d+$|^(?:PC|HMI|X2X)$|^UART\d+_CS(?:\s+channel\s+[AB])?$",
    re.IGNORECASE,
)


@dataclasses.dataclass(frozen=True)
class ParsedDiagnosticValue:
    section: str | None
    group: str | None
    scope: str | None
    key: str
    value: str
    line_number: int

    def qualified_key(self) -> str:
        parts: list[str] = []
        if self.group:
            parts.append(self.group)
        elif self.section:
            parts.append(self.section)
        if self.scope:
            parts.append(self.scope)
        parts.append(self.key)
        return ".".join(parts)


def _parse_assignment_line(line: str) -> tuple[str | None, list[tuple[str, str]]]:
    matches = list(_ASSIGNMENT.finditer(line))
    if not matches:
        return None, []

    prefix = line[: matches[0].start()].strip().rstrip(":").strip() or None
    pairs: list[tuple[str, str]] = []
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(line)
        value = line[match.end() : end].strip().rstrip(",").strip()
        pairs.append((match.group("key").casefold(), value))
    return prefix, pairs


def parse_diagnostic_output(text: str) -> list[ParsedDiagnosticValue]:
    """Parse sections, groups and every ``key = value`` without losing duplicates."""
    values: list[ParsedDiagnosticValue] = []
    section: str | None = None
    group: str | None = None
    subgroup: str | None = None

    for line_number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped or set(stripped) == {"="}:
            continue

        section_match = _SECTION_LINE.match(line)
        if section_match is not None:
            section = section_match.group(1).strip()
            group = None
            subgroup = None
            continue

        group_match = _GROUP_LINE.match(line)
        if group_match is not None:
            group = group_match.group(1).strip()
            subgroup = None
            continue

        prefix, pairs = _parse_assignment_line(line)
        if not pairs:
            continue

        if prefix and _SUBGROUP_PREFIX.fullmatch(prefix):
            subgroup = prefix

        scope = prefix or subgroup
        for key, value in pairs:
            values.append(
                ParsedDiagnosticValue(
                    section=section,
                    group=group,
                    scope=scope,
                    key=key,
                    value=value,
                    line_number=line_number,
                )
            )

    return values


def qualified_values(values: list[ParsedDiagnosticValue]) -> dict[str, str]:
    """Return a readable flat view while retaining repeated keys with suffixes."""
    output: dict[str, str] = {}
    counts: dict[str, int] = {}
    for item in values:
        base = item.qualified_key()
        key = base
        if key in output:
            counts[base] = counts.get(base, 1) + 1
            key = f"{base}#{counts[base]}"
        output[key] = item.value
    return output


class DiagnosticReport:
    """Case-insensitive lookup over a parsed diagnostic report."""

    def __init__(self, text: str) -> None:
        self.raw_output = text
        self.entries = parse_diagnostic_output(text)

    def all(
        self,
        key: str,
        *,
        group: str | None = None,
        scope: str | None = None,
    ) -> list[str]:
        key_folded = key.casefold()
        group_folded = group.casefold() if group is not None else None
        scope_folded = scope.casefold() if scope is not None else None
        output: list[str] = []
        for item in self.entries:
            if item.key != key_folded:
                continue
            if group_folded is not None and (item.group or "").casefold() != group_folded:
                continue
            if scope_folded is not None and (item.scope or "").casefold() != scope_folded:
                continue
            output.append(item.value)
        return output

    def one(
        self,
        key: str,
        *,
        group: str | None = None,
        scope: str | None = None,
    ) -> str | None:
        values = self.all(key, group=group, scope=scope)
        return values[0] if values else None

    def group_names(self, prefix: str | None = None) -> list[str]:
        names = {item.group for item in self.entries if item.group}
        if prefix is not None:
            folded = prefix.casefold()
            names = {name for name in names if name.casefold().startswith(folded)}
        return sorted(names)
