#include <iostream>
#include <cassert>
#include <libpq-fe.h>
#include "page.h"
#include "pagestore.h"
using namespace minidb;
using namespace std;

// ---- minidb side ----
void test_minidb_page() {
    // TODO: create a minidb::Page, call init()
    Page* p = new Page();

    // TODO: print pd_lower, pd_upper, pd_special, pd_pagesize_version
    PageHeader* hdr = p->header();
    cout << "pd_lower : " << hdr->pd_lower <<endl;
    cout << "pd_upper : " << hdr->pd_upper <<endl;
    cout << "pd_special : " << hdr->pd_special <<endl;
    cout << "pd_pagesize_version : " << hdr->pd_pagesize_version <<endl;

    // TODO: assert pd_lower == 24 (why 24? think about it)
    /* after pageheader the pd_lower starts immediately, so it should be 24, here the first line pointer will be inserted */
    assert(hdr->pd_lower = sizeof(PageHeader)); 
    
    // TODO: assert pd_upper == 8192
    assert(hdr->pd_upper == PAGE_SIZE);

    delete p;
}

// ---- postgres side ----
void test_postgres_page(PGconn* conn) {
    // TODO: create a temp table with one integer column
        PGresult *res = PQexec(conn, "CREATE TEMP TABLE page_test (id INT)");
        if (PQresultStatus(res) != PGRES_COMMAND_OK){
            cerr << "FAILED TO CREATE TEMP TABLE:" << PQerrorMessage(conn) << endl;
            PQclear(res);
            return;
        }

    // TODO: insert one row
    res = PQexec(conn, "INSERT INTO page_test VALUES (42)");
    if (PQresultStatus(res) != PGRES_COMMAND_OK){
        cerr << "FAILED TO INSERT INTO TEMP TABLE:" << PQerrorMessage(conn) << endl;
        PQclear(res);
        return;
    }

    PQexec(conn, "CREATE EXTENSION IF NOT EXISTS pageinspect");
    if (PQresultStatus(res) != PGRES_COMMAND_OK){
        cerr << "FAILED TO CREATE EXTENSION:" << PQerrorMessage(conn) << endl;
        PQclear(res);
        return;
    }

    // TODO: call get_raw_page() and heap_page_items() 
    res = PQexec(conn, "SELECT * FROM heap_page_items(get_raw_page('page_test', 0));");
    if (PQresultStatus(res) != PGRES_TUPLES_OK){
        cerr << "FAILED TO GET RAW PAGE:" << PQerrorMessage(conn) << endl;
        PQclear(res);
        return;
    }

    // TODO: print lp_off, lp_len, lp_flags for the first line pointer
    res = PQexec(conn, "SELECT * FROM heap_page_items(get_raw_page('page_test', 0))");
    if (PQresultStatus(res) != PGRES_TUPLES_OK){
        cerr << "FAILED TO GET LINE POINTER:" << PQerrorMessage(conn) << endl;
        PQclear(res);
        return;         
    }
    char* lp_off = PQgetvalue(res, 0, 0);
    char* lp_len = PQgetvalue(res, 0, 1);
    char* lp_flags = PQgetvalue(res, 0, 2);
    cout << "lp_off: " << lp_off << ", lp_len: " << lp_len << ", lp_flags: " << lp_flags << endl;
    
}

void test_file_page_store() {
    // TODO: create FilePageStore at /tmp/minidb_test.db
    remove("/tmp/minidb_test.db");
    FilePageStore store("/tmp/minidb_test.db");

    // TODO: allocate 3 pages
    uint32_t page0 = store.allocate_page();
    uint32_t page1 = store.allocate_page();
    uint32_t page2 = store.allocate_page();
    cout << "num_pages: " <<store.num_pages() << "\n";

    // TODO: insert a tuple into page 0
    Page page;
    uint8_t tuple_data[4] = {1, 2, 3, 4};
    uint16_t tuple_len = 4;
    uint16_t lp_index = page.insert_tuple(tuple_data, tuple_len);
    assert(lp_index == 1);
    store.write_page(page0, page);

    // // TODO: flush
    store.flush();

    // TODO: reopen the same file in a new FilePageStore
    FilePageStore store2("/tmp/minidb_test.db");

    // TODO: read page 0 back
    store2.read_page(page0, page);

    // TODO: assert tuple data matches
    uint16_t out_len;
    const uint8_t* out_data = page.get_tuple(lp_index, out_len);
    std::cout << "Read back tuple: ";
    for (uint16_t i = 0; i < out_len; i++) {
        std::cout << (int)out_data[i] << " ";
    }
    std::cout << "\n";
    assert(out_len == tuple_len);
    assert(memcmp(out_data, tuple_data, tuple_len) == 0);
}

int main() {
    // PGconn* conn = PQconnectdb("dbname=postgres user=saumyakumar host=/tmp");
    // TODO: check connection status, exit on failure

    // test_minidb_page();
    // test_postgres_page(conn);
    test_file_page_store();

    // PQfinish(conn);
    return 0;
}