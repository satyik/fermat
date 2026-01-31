#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

// Simple generic handles for Fermat (double/float based) using raw pointers
// cast to long We use 'double' in Fermat, so we can cast pointers to uint64_t
// and back.

extern "C" {

// --- IO ---
double fermat_print(double val) {
  fprintf(stdout, "%g", val);
  return 0.0;
}

double fermat_println(double val) {
  fprintf(stdout, "%g\n", val);
  return 0.0;
}

// --- ArrayList (std::vector<double>) ---

void *fermat_list_create() { return new std::vector<double>(); }

void fermat_list_free(void *ptr) {
  if (ptr)
    delete static_cast<std::vector<double> *>(ptr);
}

void fermat_list_push(void *ptr, double val) {
  if (ptr)
    static_cast<std::vector<double> *>(ptr)->push_back(val);
}

double fermat_list_get(void *ptr, double idx) {
  if (!ptr)
    return 0.0;
  auto *vec = static_cast<std::vector<double> *>(ptr);
  size_t i = (size_t)idx;
  if (i < vec->size())
    return (*vec)[i];
  return 0.0; // Error / Out of bounds
}

void fermat_list_set(void *ptr, double idx, double val) {
  if (!ptr)
    return;
  auto *vec = static_cast<std::vector<double> *>(ptr);
  size_t i = (size_t)idx;
  if (i < vec->size())
    (*vec)[i] = val;
}

double fermat_list_size(void *ptr) {
  if (!ptr)
    return 0.0;
  return (double)static_cast<std::vector<double> *>(ptr)->size();
}

// --- Map (std::map<double, double>) ---

void *fermat_map_create() { return new std::map<double, double>(); }

void fermat_map_free(void *ptr) {
  if (ptr)
    delete static_cast<std::map<double, double> *>(ptr);
}

void fermat_map_put(void *ptr, double key, double val) {
  if (ptr)
    (*static_cast<std::map<double, double> *>(ptr))[key] = val;
}

double fermat_map_get(void *ptr, double key) {
  if (!ptr)
    return 0.0;
  auto *m = static_cast<std::map<double, double> *>(ptr);
  auto it = m->find(key);
  if (it != m->end())
    return it->second;
  return 0.0; // Not found default
}

double fermat_map_check(void *ptr, double key) {
  if (!ptr)
    return 0.0;
  auto *m = static_cast<std::map<double, double> *>(ptr);
  return (m->find(key) != m->end()) ? 1.0 : 0.0;
}

double fermat_map_size(void *ptr) {
  if (!ptr)
    return 0.0;
  return (double)static_cast<std::map<double, double> *>(ptr)->size();
}

// --- Set (std::set<double>) ---

void *fermat_set_create() { return new std::set<double>(); }

void fermat_set_free(void *ptr) {
  if (ptr)
    delete static_cast<std::set<double> *>(ptr);
}

void fermat_set_add(void *ptr, double val) {
  if (ptr)
    static_cast<std::set<double> *>(ptr)->insert(val);
}

double fermat_set_contains(void *ptr, double val) {
  if (!ptr)
    return 0.0;
  auto *s = static_cast<std::set<double> *>(ptr);
  return (s->find(val) != s->end()) ? 1.0 : 0.0;
}

double fermat_set_size(void *ptr) {
  if (!ptr)
    return 0.0;
  return (double)static_cast<std::set<double> *>(ptr)->size();
}

} // extern "C"
