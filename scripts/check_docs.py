#!/usr/bin/env python3
"""Confere links, âncoras e imagens de documentos ativos, sem dependências externas."""
from dataclasses import dataclass
from html import unescape
from html.parser import HTMLParser
from pathlib import Path
import argparse
import re
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


@dataclass
class Reference:
    target: str
    image: bool = False
    alt: str = ''


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.references = []
        self.identifiers = set()

    def handle_starttag(self, tag, attrs):
        attributes = dict(attrs)
        for key in ('id', 'name'):
            if attributes.get(key):
                self.identifiers.add(attributes[key])
        if tag == 'a' and attributes.get('href'):
            self.references.append(Reference(attributes['href']))
        if tag == 'img':
            self.references.append(Reference(attributes.get('src', ''), True, attributes.get('alt', '')))


def without_fences(text):
    return re.sub(r'^```[^\n]*\n.*?^```\s*$', '', text, flags=re.M | re.S)


def references(text):
    text = without_fences(text)
    html = Links()
    html.feed(text)
    found = list(html.references)
    for match in re.finditer(r'(!?)\[([^\]]*)\]\((<[^>]+>|[^\s)]+)(?:\s+"[^"]*")?\)', text):
        found.append(Reference(match[3].strip('<>'), bool(match[1]), match[2]))
    return found


def anchors(text):
    text = without_fences(text)
    html = Links()
    html.feed(text)
    result = set(html.identifiers)
    counts = {}
    for heading in re.findall(r'^#{1,6}\s+(.+?)(?:\s+#+)?\s*$', text, flags=re.M):
        heading = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', heading)
        plain = unescape(re.sub(r'<[^>]*>', '', heading)).lower()
        slug = re.sub(r'[^\w\- ]', '', plain).replace(' ', '-')
        count = counts.get(slug, 0)
        counts[slug] = count + 1
        result.add(f'{slug}-{count}' if count else slug)
    return result


def documents(root):
    paths = [root / 'README.md', root / 'CHANGELOG.md', root / 'third_party/README.md']
    paths.extend((root / 'docs').rglob('*.md'))
    paths.extend((root / 'assets').rglob('*.md'))
    # Materiais anteriores seguem preservados, sem impor suas instruções ao build atual.
    return sorted({p for p in paths if p.is_file() and not (
        'legacy' in p.relative_to(root).parts and p.name != 'README.md')})


def check(root=ROOT, paths=None):
    root = Path(root).resolve()
    errors = []
    paths = documents(root) if paths is None else paths
    for document in paths:
        document = Path(document).resolve()
        text = document.read_text(encoding='utf-8')
        label = str(document.relative_to(root))
        if len(re.findall(r'^```', text, flags=re.M)) % 2:
            errors.append(f'{label}: bloco de código sem fechamento')
        for ref in references(text):
            if ref.image and not ref.alt.strip():
                errors.append(f'{label}: imagem sem texto alternativo')
            if not ref.target:
                errors.append(f'{label}: referência vazia')
                continue
            target = urlsplit(unescape(ref.target))
            if target.scheme in ('https', 'http', 'mailto'):
                continue
            if target.scheme or target.netloc:
                errors.append(f'{label}: esquema de referência não suportado: {ref.target}')
                continue
            destination = (document.parent / unquote(target.path)).resolve() if target.path else document
            if not destination.is_relative_to(root):
                errors.append(f'{label}: caminho sai do repositório: {ref.target}')
                continue
            if not destination.exists():
                errors.append(f'{label}: arquivo inexistente: {ref.target}')
                continue
            if ref.image and (not destination.is_file() or not destination.stat().st_size):
                errors.append(f'{label}: imagem vazia ou inválida: {ref.target}')
            if target.fragment and destination.suffix.lower() == '.md':
                if unquote(target.fragment) not in anchors(destination.read_text(encoding='utf-8')):
                    errors.append(f'{label}: âncora inexistente: {ref.target}')
    return errors


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--external-list', action='store_true', help='Lista URLs para verificação externa separada')
    args = parser.parse_args()
    if args.external_list:
        for url in sorted({ref.target for p in documents(ROOT) for ref in references(p.read_text())
                           if urlsplit(ref.target).scheme in ('http', 'https')}):
            print(url)
    else:
        failures = check()
        for failure in failures:
            print(failure)
        if failures:
            raise SystemExit(1)
        print(f'Documentação válida: {len(documents(ROOT))} arquivos; links locais, âncoras e imagens conferidos.')
