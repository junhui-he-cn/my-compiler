#pragma once

#include "TypeCheckerTypes.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

using ModuleValueExports = std::unordered_map<std::string, TypeBinding>;
using ModuleStructExports = std::unordered_map<std::string, StructTypeDecl>;

struct NamespaceImport {
    ModuleValueExports values;
    ModuleStructExports structs;
};

class ModuleSymbols {
public:
    void clear();

    bool markDirectImport(std::size_t importingModuleId, std::size_t importedModuleId);

    void recordValueExport(std::size_t moduleId, std::string name, TypeBinding binding);
    const ModuleValueExports* valueExports(std::size_t moduleId) const;

    void markLocalStruct(std::size_t moduleId, const std::string& name);
    bool isLocalStruct(std::size_t moduleId, const std::string& name) const;
    void recordStructExport(std::size_t moduleId, std::string name, StructTypeDecl declaration);
    const ModuleStructExports* structExports(std::size_t moduleId) const;

    bool hasNamespace(std::size_t moduleId, const std::string& alias) const;
    void recordNamespace(std::size_t moduleId, std::string alias, NamespaceImport imported);
    const NamespaceImport* namespaceImport(std::size_t moduleId, const std::string& alias) const;

private:
    std::unordered_map<std::size_t, ModuleValueExports> valueExports_;
    std::unordered_map<std::size_t, ModuleStructExports> structExports_;
    std::unordered_map<std::size_t, std::unordered_set<std::string>> localStructNames_;
    std::unordered_map<std::size_t, std::unordered_map<std::string, NamespaceImport>> namespaces_;
    std::unordered_map<std::size_t, std::unordered_set<std::size_t>> directImports_;
};
