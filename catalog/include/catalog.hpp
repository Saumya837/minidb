#pragma once

#include<string>
#include<vector>
#include<unordered_map>
#include<optional>

std::string foldIdentifier(std::string name);

enum class DataType{
    INT, 
    FLOAT, 
    TEXT
};

std::string typeToString(DataType datatype);

struct ColumnDef{
    std::string name;
    DataType type;
    bool nullable;
};


class TableDef {
    private:
        std::string _name;
        std::vector<ColumnDef> _columns;

    public: 
        TableDef(const std::string& name, std::vector<ColumnDef> columns);
        const std::string& name() const;
        size_t columnCount() const;
        const ColumnDef& columnAt(size_t pos) const;
        const std::vector<ColumnDef>& columns() const;
        std::optional<size_t> findColumn(const std::string& name) const;
};


class Catalog {
    private:
        std::unordered_map<std::string, TableDef> tables_;

    public:
        void addTable(const std::string& name, std::vector<ColumnDef> columns);
        const TableDef* getTable(const std::string& name) const;
        const std::unordered_map<std::string, TableDef>& tables() const;

};


