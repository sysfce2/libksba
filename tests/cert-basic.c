/* cert-basic.c - basic test for the certificate management.
 *      Copyright (C) 2001, 2002 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * KSBA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../src/ksba.h"
#include "../src/keyinfo.h"

#define digitp(p)   (*(p) >= '0' && *(p) <= '9')

#define fail_if_err(a) do { if(a) {                                       \
                              fprintf (stderr, "%s:%d: KSBA error: %s\n", \
                              __FILE__, __LINE__, gpg_strerror(a));   \
                              exit (1); }                              \
                           } while(0)


#define fail_if_err2(f, a) do { if(a) {\
            fprintf (stderr, "%s:%d: KSBA error on file `%s': %s\n", \
                       __FILE__, __LINE__, (f), gpg_strerror(a));   \
                            exit (1); }                              \
                           } while(0)

#define xfree(a)  ksba_free (a)

static int errorcount = 0;


static void *
xmalloc (size_t n)
{
  char *p = ksba_malloc (n);
  if (!p)
    {
      fprintf (stderr, "out of core\n");
      exit (1);
    }
  return p;
}


static void
print_sexp (ksba_const_sexp_t p)
{
  int level = 0;

  if (!p)
    fputs ("[none]", stdout);
  else
    {
      for (;;)
        {
          if (*p == '(')
            {
              putchar (*p);
              p++;
              level++;
            }
          else if (*p == ')')
            {
              putchar (*p);
              if (--level <= 0 )
                return;
            }
          else if (!digitp (p))
            {
              fputs ("[invalid s-exp]", stdout);
              return;
            }
          else
            {
              char *endp;
              unsigned long n;

              n = strtoul (p, &endp, 10);
              p = endp;
              if (*p != ':')
                {
                  fputs ("[invalid s-exp]", stdout);
                  return;
                }
              putchar('#');
              for (p++; n; n--, p++)
                printf ("%02X", *p);
              putchar('#');
            }
        }
    }
}

static void
print_time (ksba_isotime_t t)
{
  if (!t || !*t)
    fputs ("none", stdout);
  else
    printf ("%.4s-%.2s-%.2s %.2s:%.2s:%s", t, t+4, t+6, t+9, t+11, t+13);
}

static void
print_dn (char *p)
{

  if (!p)
    fputs ("error", stdout);
  else
    printf ("`%s'", p);
}

static void
print_names (int indent, ksba_name_t name)
{
  int idx;
  const char *s;

  if (!name)
    {
      fputs ("none\n", stdout);
      return;
    }
  
  for (idx=0; (s = ksba_name_enum (name, idx)); idx++)
    {
      char *p = ksba_name_get_uri (name, idx);
      printf ("%*s%s\n", idx?indent:0, "", p?p:s);
      xfree (p);
    }
}


static void
list_extensions (ksba_cert_t cert)
{
  gpg_error_t err;
  const char *oid;
  int idx, crit, is_ca, pathlen;
  size_t off, len;
  unsigned int usage, reason;
  char *string, *p;
  ksba_name_t name1, name2;
  ksba_sexp_t serial;

  for (idx=0; !(err=ksba_cert_get_extension (cert, idx,
                                             &oid, &crit, &off, &len));idx++)
    {
      printf ("Extn: %s at %d with length %d %s\n",
              oid, (int)off, (int)len, crit? "(critical)":"");
    }
  if (err && gpg_err_code (err) != GPG_ERR_EOF )
    {
      fprintf (stderr,
               "%s:%d: enumerating extensions failed: %s\n", 
               __FILE__, __LINE__, gpg_strerror (err));
      errorcount++;
    }

  /* authorithyKeyIdentifier */
  err = ksba_cert_get_auth_key_id (cert, NULL, &name1, &serial);
  if (!err || gpg_err_code (err) == GPG_ERR_NO_DATA)
    {
      fputs ("AuthorithyKeyIdentifier: ", stdout);
      if (gpg_err_code (err) == GPG_ERR_NO_DATA)
        fputs ("none", stdout);
      else
        {
          print_names (25, name1);
          ksba_name_release (name1);
          fputs ("                 serial: ", stdout);
          print_sexp (serial);
          ksba_free (serial);
        }
      putchar ('\n');
    }
  else
    { 
      fprintf (stderr, "%s:%d: ksba_cert_get_auth_key_id: %s\n", 
               __FILE__, __LINE__, gpg_strerror (err));
      errorcount++;
    } 

  err = ksba_cert_is_ca (cert, &is_ca, &pathlen);
  if (err)
    {
      fprintf (stderr, "%s:%d: ksba_cert_is_ca failed: %s\n", 
               __FILE__, __LINE__, gpg_strerror (err));
      errorcount++;
    }
  else if (is_ca)
    printf ("This is a CA certificate with a path length of %d\n", pathlen);

  err = ksba_cert_get_key_usage (cert, &usage);
  if (gpg_err_code (err) == GPG_ERR_NO_DATA)
    printf ("KeyUsage: Not specified\n");
  else if (err)
    { 
      fprintf (stderr, "%s:%d: ksba_cert_get_key_usage failed: %s\n", 
               __FILE__, __LINE__, gpg_strerror (err));
      errorcount++;
    } 
  else
    {
      fputs ("KeyUsage:", stdout);
      if ( (usage & KSBA_KEYUSAGE_DIGITAL_SIGNATURE))
        fputs (" digitalSignature", stdout);
      if ( (usage & KSBA_KEYUSAGE_NON_REPUDIATION))  
        fputs (" nonRepudiation", stdout);
      if ( (usage & KSBA_KEYUSAGE_KEY_ENCIPHERMENT)) 
        fputs (" keyEncipherment", stdout);
      if ( (usage & KSBA_KEYUSAGE_DATA_ENCIPHERMENT))
        fputs (" dataEncripherment", stdout);
      if ( (usage & KSBA_KEYUSAGE_KEY_AGREEMENT))    
        fputs (" keyAgreement", stdout);
      if ( (usage & KSBA_KEYUSAGE_KEY_CERT_SIGN))
        fputs (" certSign", stdout);
      if ( (usage & KSBA_KEYUSAGE_CRL_SIGN))  
        fputs (" crlSign", stdout);
      if ( (usage & KSBA_KEYUSAGE_ENCIPHER_ONLY))
        fputs (" encipherOnly", stdout);
      if ( (usage & KSBA_KEYUSAGE_DECIPHER_ONLY))  
        fputs (" decipherOnly", stdout);
      putchar ('\n');
    }

  err = ksba_cert_get_cert_policies (cert, &string);
  if (gpg_err_code (err) == GPG_ERR_NO_DATA)
    printf ("CertificatePolicies: none\n");
  else if (err)
    { 
      fprintf (stderr, "%s:%d: ksba_cert_get_cert_policies failed: %s\n", 
               __FILE__, __LINE__, gpg_strerror (err));
      errorcount++;
    } 
  else
    {
      /* for display purposes we replace the linefeeds by commas */
      for (p=string; *p; p++)
        {
          if (*p == '\n')
            *p = ',';
        }
      printf ("CertificatePolicies: %s\n", string);
      xfree (string);
    }

  /* CRL distribution point */
  for (idx=0; !(err=ksba_cert_get_crl_dist_point (cert, idx,
                                                  &name1, &name2,
                                                  &reason));idx++)
    {
      fputs ("CRLDistPoint: ", stdout);
      print_names (14, name1);
      fputs ("     reasons:", stdout);
      if ( !reason )
        fputs (" none", stdout);
      if ( (reason & KSBA_CRLREASON_UNSPECIFIED))
        fputs (" unused", stdout);
      if ( (reason & KSBA_CRLREASON_KEY_COMPROMISE))
        fputs (" keyCompromise", stdout);
      if ( (reason & KSBA_CRLREASON_CA_COMPROMISE))
        fputs (" caCompromise", stdout);
      if ( (reason & KSBA_CRLREASON_AFFILIATION_CHANGED))
        fputs (" affiliationChanged", stdout);
      if ( (reason & KSBA_CRLREASON_SUPERSEDED))
        fputs (" superseded", stdout);
      if ( (reason & KSBA_CRLREASON_CESSATION_OF_OPERATION))
        fputs (" cessationOfOperation", stdout);
      if ( (reason & KSBA_CRLREASON_CERTIFICATE_HOLD))
        fputs (" certificateHold", stdout);
      putchar ('\n');
      fputs ("      issuer: ", stdout);
      print_names (14, name2);
      ksba_name_release (name1);
      ksba_name_release (name2);
    }
  if (err && gpg_err_code (err) != GPG_ERR_EOF)
    { 
      fprintf (stderr, "%s:%d: ksba_cert_get_crl_dist_point failed: %s\n", 
               __FILE__, __LINE__, gpg_strerror (err));
      errorcount++;
    } 

}


static void
one_file (const char *fname)
{
  gpg_error_t err;
  FILE *fp;
  ksba_reader_t r;
  ksba_cert_t cert;
  char *dn;
  ksba_isotime_t t;
  int idx;
  ksba_sexp_t sexp;

  fp = fopen (fname, "r");
  if (!fp)
    {
      fprintf (stderr, "%s:%d: can't open `%s': %s\n", 
               __FILE__, __LINE__, fname, strerror (errno));
      exit (1);
    }

  err = ksba_reader_new (&r);
  if (err)
    fail_if_err (err);
  err = ksba_reader_set_file (r, fp);
  fail_if_err (err);

  err = ksba_cert_new (&cert);
  if (err)
    fail_if_err (err);

  err = ksba_cert_read_der (cert, r);
  fail_if_err2 (fname, err);

  printf ("Certificate in `%s':\n", fname);

  sexp = ksba_cert_get_serial (cert);
  fputs ("  serial....: ", stdout);
  print_sexp (sexp);
  ksba_free (sexp);
  putchar ('\n');

  for (idx=0;(dn = ksba_cert_get_issuer (cert, idx));idx++) 
    {
      fputs (idx?"         aka: ":"  issuer....: ", stdout);
      print_dn (dn);
      ksba_free (dn);
      putchar ('\n');
    }

  for (idx=0;(dn = ksba_cert_get_subject (cert, idx));idx++) 
    {
      fputs (idx?"         aka: ":"  subject...: ", stdout);
      print_dn (dn);
      ksba_free (dn);
      putchar ('\n');
    }

  ksba_cert_get_validity (cert, 0, t);
  fputs ("  notBefore.: ", stdout);
  print_time (t);
  putchar ('\n');
  ksba_cert_get_validity (cert, 1, t);
  fputs ("  notAfter..: ", stdout);
  print_time (t);
  putchar ('\n');

  printf ("  hash algo.: %s\n", ksba_cert_get_digest_algo (cert));

  /* check that the sexp to keyinfo conversion works */
  {
    ksba_sexp_t public;

    public = ksba_cert_get_public_key (cert);
    if (!public)
      {
        fprintf (stderr, "%s:%d: public key not found\n", 
                 __FILE__, __LINE__);
        errorcount++;
      }
    else
      {
        unsigned char *der;
        size_t derlen;

        err = _ksba_keyinfo_from_sexp (public, &der, &derlen);
        if (err)
          {
            fprintf (stderr, "%s:%d: converting public key failed: %s\n", 
                     __FILE__, __LINE__, gpg_strerror (err));
            errorcount++;
          }
        else 
          {
            ksba_sexp_t tmp;

            err = _ksba_keyinfo_to_sexp (der, derlen, &tmp);
            if (err)
              {
                fprintf (stderr,
                         "%s:%d: re-converting public key failed: %s\n", 
                         __FILE__, __LINE__, gpg_strerror (err));
                errorcount++;
              }
            else
              {
                unsigned char *der2;
                size_t derlen2;

                err = _ksba_keyinfo_from_sexp (tmp, &der2, &derlen2);
                if (err)
                  {
                    fprintf (stderr, "%s:%d: re-re-converting "
                             "public key failed: %s\n", 
                             __FILE__, __LINE__, gpg_strerror (err));
                    errorcount++;
                  }
                else if (derlen != derlen2 || memcmp (der, der2, derlen))
                  {
                    fprintf (stderr, "%s:%d: mismatch after "
                             "re-re-converting public key\n", 
                             __FILE__, __LINE__);
                    errorcount++;
                    xfree (der2);
                  }
                xfree (tmp);
              }
            xfree (der);
          }
      }
  }

  list_extensions (cert);

#if 0
  sexp = ksba_cert_get_sig_val (cert);
  fputs ("  sigval....: ", stdout);
  print_sexp (sexp);
  ksba_free (sexp);
  putchar ('\n');
#endif

  ksba_cert_release (cert);
  err = ksba_cert_new (&cert);
  if (err)
    fail_if_err (err);

  err = ksba_cert_read_der (cert, r);
  if (gpg_err_code (err) != GPG_ERR_EOF)
    {
      fprintf (stderr, "%s:%d: expected EOF but got: %s\n", 
               __FILE__, __LINE__, gpg_strerror (err));
      errorcount++;
    }

  putchar ('\n');
  ksba_cert_release (cert);
  ksba_reader_release (r);
  fclose (fp);
}




int 
main (int argc, char **argv)
{
  const char *srcdir = getenv ("srcdir");
  
  if (!srcdir)
    srcdir = ".";

  if (argc > 1)
    {
      for (argc--, argv++; argc; argc--, argv++)
        one_file (*argv);
    }
  else
    {
      const char *files[] = {
        "cert_dfn_pca01.der",
        "cert_dfn_pca15.der",
        "cert_g10code_test1.der",
        NULL 
      };
      int idx;
      
      for (idx=0; files[idx]; idx++)
        {
          char *fname;

          fname = xmalloc (strlen (srcdir) + 1 + strlen (files[idx]) + 1);
          strcpy (fname, srcdir);
          strcat (fname, "/");
          strcat (fname, files[idx]);
          one_file (fname);
          ksba_free (fname);
        }
    }

  return !!errorcount;
}

